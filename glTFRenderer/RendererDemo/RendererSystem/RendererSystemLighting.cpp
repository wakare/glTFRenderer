// DX use [0, 1] as depth clip range
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "RendererSystemLighting.h"
#include "RenderPassSetupBuilder.h"
#include "RendererSystemSSAO.h"
#include "SceneFileLoader/glTFImageIOUtil.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <limits>
#include <imgui/imgui.h>

#include "RendererSceneAABB.h"

namespace
{
    constexpr float CPU_PI = 3.14159265358979323846f;
    constexpr unsigned ENVIRONMENT_IRRADIANCE_WIDTH = 64u;
    constexpr unsigned ENVIRONMENT_IRRADIANCE_HEIGHT = 32u;
    constexpr unsigned ENVIRONMENT_BRDF_LUT_WIDTH = 128u;
    constexpr unsigned ENVIRONMENT_BRDF_LUT_HEIGHT = 128u;
    constexpr std::array<unsigned, 4> ENVIRONMENT_PREFILTER_WIDTHS = {128u, 64u, 32u, 16u};
    constexpr std::array<unsigned, 4> ENVIRONMENT_PREFILTER_HEIGHTS = {64u, 32u, 16u, 8u};
    constexpr std::array<float, 4> ENVIRONMENT_PREFILTER_ROUGHNESS_LEVELS = {0.25f, 0.50f, 0.75f, 1.00f};
    constexpr unsigned ENVIRONMENT_IRRADIANCE_SAMPLE_COUNT = 48u;
    constexpr unsigned ENVIRONMENT_PREFILTER_SAMPLE_COUNT = 32u;
    constexpr unsigned ENVIRONMENT_BRDF_SAMPLE_COUNT = 128u;

    constexpr std::array<char, 4> FALLBACK_ENVIRONMENT_TEXTURE_RGBA = {
        static_cast<char>(191),
        static_cast<char>(194),
        static_cast<char>(204),
        static_cast<char>(255)
    };

    struct LinearEnvironmentImage
    {
        unsigned width{0u};
        unsigned height{0u};
        std::vector<glm::fvec3> pixels{};

        bool IsValid() const
        {
            return width > 0u && height > 0u &&
                   pixels.size() == static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        }
    };

    struct LinearFloat4Image
    {
        unsigned width{0u};
        unsigned height{0u};
        std::vector<glm::fvec4> pixels{};

        bool IsValid() const
        {
            return width > 0u && height > 0u &&
                   pixels.size() == static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        }
    };

    float SrgbToLinearChannel(float value)
    {
        if (value <= 0.04045f)
        {
            return value / 12.92f;
        }

        return std::pow((value + 0.055f) / 1.055f, 2.4f);
    }

    glm::fvec3 DecodeImagePixelToLinear(const ImageLoadResult& image, std::size_t pixel_index)
    {
        const std::size_t base = pixel_index * 4u;
        if (image.format == GUID_WICPixelFormat32bppRGBA)
        {
            return glm::fvec3(
                SrgbToLinearChannel(static_cast<float>(image.data[base + 0u]) / 255.0f),
                SrgbToLinearChannel(static_cast<float>(image.data[base + 1u]) / 255.0f),
                SrgbToLinearChannel(static_cast<float>(image.data[base + 2u]) / 255.0f));
        }

        if (image.format == GUID_WICPixelFormat32bppBGRA)
        {
            return glm::fvec3(
                SrgbToLinearChannel(static_cast<float>(image.data[base + 2u]) / 255.0f),
                SrgbToLinearChannel(static_cast<float>(image.data[base + 1u]) / 255.0f),
                SrgbToLinearChannel(static_cast<float>(image.data[base + 0u]) / 255.0f));
        }

        return glm::fvec3(0.0f);
    }

    bool LoadLinearEnvironmentImage(const std::string& texture_uri, LinearEnvironmentImage& out_image)
    {
        ImageLoadResult loaded_image{};
        const std::wstring texture_path = to_wide_string(texture_uri);
        if (!glTFImageIOUtil::Instance().LoadImageByFilename(texture_path.c_str(), loaded_image))
        {
            return false;
        }

        if (loaded_image.format != GUID_WICPixelFormat32bppRGBA &&
            loaded_image.format != GUID_WICPixelFormat32bppBGRA)
        {
            return false;
        }

        out_image.width = loaded_image.width;
        out_image.height = loaded_image.height;
        out_image.pixels.resize(static_cast<std::size_t>(loaded_image.width) * static_cast<std::size_t>(loaded_image.height));
        for (std::size_t pixel_index = 0; pixel_index < out_image.pixels.size(); ++pixel_index)
        {
            out_image.pixels[pixel_index] = DecodeImagePixelToLinear(loaded_image, pixel_index);
        }

        return out_image.IsValid();
    }

    std::size_t WrapIndex(int value, std::size_t extent)
    {
        const int extent_int = static_cast<int>(extent);
        int wrapped = value % extent_int;
        if (wrapped < 0)
        {
            wrapped += extent_int;
        }

        return static_cast<std::size_t>(wrapped);
    }

    glm::fvec3 BilinearSampleLatLong(const LinearEnvironmentImage& image, float u, float v)
    {
        if (!image.IsValid())
        {
            return glm::fvec3(0.0f);
        }

        const float wrapped_u = u - std::floor(u);
        const float clamped_v = (std::max)(0.0f, (std::min)(v, 1.0f));
        const float x = wrapped_u * static_cast<float>(image.width) - 0.5f;
        const float y = clamped_v * static_cast<float>(image.height) - 0.5f;
        const int x0 = static_cast<int>(std::floor(x));
        const int y0 = static_cast<int>(std::floor(y));
        const int x1 = x0 + 1;
        const int y1 = (std::min)(y0 + 1, static_cast<int>(image.height) - 1);
        const float tx = x - static_cast<float>(x0);
        const float ty = y - static_cast<float>(y0);

        const auto sample_pixel = [&](int sample_x, int sample_y) -> glm::fvec3
        {
            const std::size_t wrapped_x = WrapIndex(sample_x, image.width);
            const std::size_t clamped_y = static_cast<std::size_t>((std::max)(0, (std::min)(sample_y, static_cast<int>(image.height) - 1)));
            return image.pixels[clamped_y * image.width + wrapped_x];
        };

        const glm::fvec3 c00 = sample_pixel(x0, y0);
        const glm::fvec3 c10 = sample_pixel(x1, y0);
        const glm::fvec3 c01 = sample_pixel(x0, y1);
        const glm::fvec3 c11 = sample_pixel(x1, y1);
        return glm::mix(glm::mix(c00, c10, tx), glm::mix(c01, c11, tx), ty);
    }

    glm::fvec3 DirectionFromLatLong(float u, float v)
    {
        const float phi = (u - 0.5f) * (2.0f * CPU_PI);
        const float theta = v * CPU_PI;
        const float sin_theta = std::sin(theta);
        return glm::normalize(glm::fvec3(
            std::cos(phi) * sin_theta,
            std::cos(theta),
            std::sin(phi) * sin_theta));
    }

    glm::fvec2 DirectionToLatLongUV(const glm::fvec3& direction)
    {
        const glm::fvec3 normalized_direction = glm::normalize(direction);
        return glm::fvec2(
            std::atan2(normalized_direction.z, normalized_direction.x) / (2.0f * CPU_PI) + 0.5f,
            std::acos((std::max)(-1.0f, (std::min)(normalized_direction.y, 1.0f))) / CPU_PI);
    }

    glm::fvec3 SampleEnvironmentDirection(const LinearEnvironmentImage& image, const glm::fvec3& direction)
    {
        const glm::fvec2 uv = DirectionToLatLongUV(direction);
        return BilinearSampleLatLong(image, uv.x, uv.y);
    }

    void BuildOrthonormalBasis(const glm::fvec3& n, glm::fvec3& tangent, glm::fvec3& bitangent)
    {
        const glm::fvec3 up = std::abs(n.y) < 0.999f ? glm::fvec3(0.0f, 1.0f, 0.0f) : glm::fvec3(1.0f, 0.0f, 0.0f);
        tangent = glm::normalize(glm::cross(up, n));
        bitangent = glm::normalize(glm::cross(n, tangent));
    }

    float RadicalInverseVdc(unsigned bits)
    {
        bits = (bits << 16) | (bits >> 16);
        bits = ((bits & 0x55555555u) << 1) | ((bits & 0xAAAAAAAAu) >> 1);
        bits = ((bits & 0x33333333u) << 2) | ((bits & 0xCCCCCCCCu) >> 2);
        bits = ((bits & 0x0F0F0F0Fu) << 4) | ((bits & 0xF0F0F0F0u) >> 4);
        bits = ((bits & 0x00FF00FFu) << 8) | ((bits & 0xFF00FF00u) >> 8);
        return static_cast<float>(bits) * 2.3283064365386963e-10f;
    }

    glm::fvec2 Hammersley2D(unsigned sample_index, unsigned sample_count)
    {
        return glm::fvec2(
            static_cast<float>(sample_index) / static_cast<float>((std::max)(sample_count, 1u)),
            RadicalInverseVdc(sample_index));
    }

    glm::fvec3 TransformLocalToWorld(const glm::fvec3& local_dir, const glm::fvec3& n)
    {
        glm::fvec3 tangent{};
        glm::fvec3 bitangent{};
        BuildOrthonormalBasis(n, tangent, bitangent);
        return glm::normalize(local_dir.x * tangent + local_dir.y * n + local_dir.z * bitangent);
    }

    glm::fvec3 SampleCosineHemisphere(unsigned sample_index, unsigned sample_count)
    {
        const glm::fvec2 xi = Hammersley2D(sample_index, sample_count);
        const float phi = 2.0f * CPU_PI * xi.x;
        const float cos_theta = std::sqrt(1.0f - xi.y);
        const float sin_theta = std::sqrt(xi.y);
        return glm::fvec3(std::cos(phi) * sin_theta, cos_theta, std::sin(phi) * sin_theta);
    }

    glm::fvec3 SampleCone(unsigned sample_index, unsigned sample_count, float cos_theta_max)
    {
        const glm::fvec2 xi = Hammersley2D(sample_index, sample_count);
        const float phi = 2.0f * CPU_PI * xi.x;
        const float cos_theta = 1.0f - xi.y * (1.0f - cos_theta_max);
        const float sin_theta = std::sqrt((std::max)(0.0f, 1.0f - cos_theta * cos_theta));
        return glm::fvec3(std::cos(phi) * sin_theta, cos_theta, std::sin(phi) * sin_theta);
    }

    std::uint16_t FloatToHalfBits(float value)
    {
        std::uint32_t bits = 0u;
        std::memcpy(&bits, &value, sizeof(bits));

        const std::uint32_t sign = (bits >> 16u) & 0x8000u;
        std::uint32_t exponent = (bits >> 23u) & 0xffu;
        std::uint32_t mantissa = bits & 0x007fffffu;

        if (exponent == 0xffu)
        {
            return static_cast<std::uint16_t>(sign | (mantissa != 0u ? 0x7e00u : 0x7c00u));
        }

        const int half_exponent = static_cast<int>(exponent) - 127 + 15;
        if (half_exponent >= 31)
        {
            return static_cast<std::uint16_t>(sign | 0x7c00u);
        }

        if (half_exponent <= 0)
        {
            if (half_exponent < -10)
            {
                return static_cast<std::uint16_t>(sign);
            }

            mantissa = (mantissa | 0x00800000u) >> (1 - half_exponent);
            return static_cast<std::uint16_t>(sign | ((mantissa + 0x00001000u) >> 13u));
        }

        return static_cast<std::uint16_t>(
            sign |
            (static_cast<std::uint32_t>(half_exponent) << 10u) |
            ((mantissa + 0x00001000u) >> 13u));
    }

    float GeometrySchlickGGXIBL(float ndotv, float roughness)
    {
        const float alpha = roughness * roughness;
        const float k = alpha * 0.5f;
        return ndotv / (ndotv * (1.0f - k) + k);
    }

    float GeometrySmithIBL(float ndotv, float ndotl, float roughness)
    {
        return GeometrySchlickGGXIBL(ndotv, roughness) * GeometrySchlickGGXIBL(ndotl, roughness);
    }

    glm::fvec3 ImportanceSampleGGX(const glm::fvec2& xi, const glm::fvec3& normal, float roughness)
    {
        const float alpha = roughness * roughness;
        const float phi = 2.0f * CPU_PI * xi.x;
        const float cos_theta = std::sqrt((1.0f - xi.y) / (1.0f + ((alpha * alpha) - 1.0f) * xi.y));
        const float sin_theta = std::sqrt((std::max)(0.0f, 1.0f - cos_theta * cos_theta));
        const glm::fvec3 local_half_vector(
            std::cos(phi) * sin_theta,
            std::sin(phi) * sin_theta,
            cos_theta);

        glm::fvec3 tangent{};
        glm::fvec3 bitangent{};
        BuildOrthonormalBasis(normal, tangent, bitangent);
        return glm::normalize(
            tangent * local_half_vector.x +
            bitangent * local_half_vector.y +
            normal * local_half_vector.z);
    }

    glm::fvec2 IntegrateEnvironmentBrdf(float ndotv, float roughness)
    {
        const glm::fvec3 normal(0.0f, 0.0f, 1.0f);
        const glm::fvec3 view(std::sqrt((std::max)(0.0f, 1.0f - ndotv * ndotv)), 0.0f, ndotv);
        glm::fvec2 integrated(0.0f);

        for (unsigned sample_index = 0; sample_index < ENVIRONMENT_BRDF_SAMPLE_COUNT; ++sample_index)
        {
            const glm::fvec3 half_vector = ImportanceSampleGGX(
                Hammersley2D(sample_index, ENVIRONMENT_BRDF_SAMPLE_COUNT),
                normal,
                roughness);
            const glm::fvec3 light = glm::normalize(2.0f * glm::dot(view, half_vector) * half_vector - view);

            const float ndotl = (std::max)(light.z, 0.0f);
            const float ndoth = (std::max)(half_vector.z, 0.0f);
            const float vdoth = (std::max)(glm::dot(view, half_vector), 0.0f);
            if (ndotl <= 0.0f)
            {
                continue;
            }

            const float geometry = GeometrySmithIBL(ndotv, ndotl, roughness);
            const float geometry_visibility = (geometry * vdoth) / (std::max)(ndoth * ndotv, 1e-5f);
            const float fresnel_weight = std::pow(1.0f - vdoth, 5.0f);
            integrated.x += (1.0f - fresnel_weight) * geometry_visibility;
            integrated.y += fresnel_weight * geometry_visibility;
        }

        return integrated / static_cast<float>(ENVIRONMENT_BRDF_SAMPLE_COUNT);
    }

    LinearEnvironmentImage GenerateIrradianceEnvironment(const LinearEnvironmentImage& source, unsigned output_width, unsigned output_height)
    {
        LinearEnvironmentImage image{};
        image.width = output_width;
        image.height = output_height;
        image.pixels.resize(static_cast<std::size_t>(output_width) * static_cast<std::size_t>(output_height));
        for (unsigned y = 0; y < output_height; ++y)
        {
            for (unsigned x = 0; x < output_width; ++x)
            {
                const glm::fvec3 normal = DirectionFromLatLong(
                    (static_cast<float>(x) + 0.5f) / static_cast<float>(output_width),
                    (static_cast<float>(y) + 0.5f) / static_cast<float>(output_height));
                glm::fvec3 radiance_sum(0.0f);
                float weight_sum = 0.0f;
                for (unsigned sample_index = 0; sample_index < ENVIRONMENT_IRRADIANCE_SAMPLE_COUNT; ++sample_index)
                {
                    const glm::fvec3 local_dir = SampleCosineHemisphere(sample_index, ENVIRONMENT_IRRADIANCE_SAMPLE_COUNT);
                    const glm::fvec3 world_dir = TransformLocalToWorld(local_dir, normal);
                    radiance_sum += SampleEnvironmentDirection(source, world_dir) * local_dir.y;
                    weight_sum += local_dir.y;
                }

                image.pixels[static_cast<std::size_t>(y) * output_width + x] =
                    weight_sum > 1e-6f ? radiance_sum / weight_sum : glm::fvec3(0.0f);
            }
        }

        return image;
    }

    LinearEnvironmentImage GeneratePrefilterEnvironment(
        const LinearEnvironmentImage& source,
        unsigned output_width,
        unsigned output_height,
        float roughness)
    {
        LinearEnvironmentImage image{};
        image.width = output_width;
        image.height = output_height;
        image.pixels.resize(static_cast<std::size_t>(output_width) * static_cast<std::size_t>(output_height));
        const float roughness_sq = roughness * roughness;
        const float cone_cos_theta_max = std::cos(roughness_sq * 0.5f * CPU_PI);
        for (unsigned y = 0; y < output_height; ++y)
        {
            for (unsigned x = 0; x < output_width; ++x)
            {
                const glm::fvec3 reflection_dir = DirectionFromLatLong(
                    (static_cast<float>(x) + 0.5f) / static_cast<float>(output_width),
                    (static_cast<float>(y) + 0.5f) / static_cast<float>(output_height));
                glm::fvec3 radiance_sum(0.0f);
                float weight_sum = 0.0f;
                for (unsigned sample_index = 0; sample_index < ENVIRONMENT_PREFILTER_SAMPLE_COUNT; ++sample_index)
                {
                    const glm::fvec3 local_dir = SampleCone(sample_index, ENVIRONMENT_PREFILTER_SAMPLE_COUNT, cone_cos_theta_max);
                    const glm::fvec3 world_dir = TransformLocalToWorld(local_dir, reflection_dir);
                    radiance_sum += SampleEnvironmentDirection(source, world_dir) * local_dir.y;
                    weight_sum += local_dir.y;
                }

                image.pixels[static_cast<std::size_t>(y) * output_width + x] =
                    weight_sum > 1e-6f ? radiance_sum / weight_sum : glm::fvec3(0.0f);
            }
        }

        return image;
    }

    LinearFloat4Image GenerateEnvironmentBrdfLut(unsigned output_width, unsigned output_height)
    {
        LinearFloat4Image image{};
        image.width = output_width;
        image.height = output_height;
        image.pixels.resize(static_cast<std::size_t>(output_width) * static_cast<std::size_t>(output_height));
        for (unsigned y = 0; y < output_height; ++y)
        {
            const float roughness = (static_cast<float>(y) + 0.5f) / static_cast<float>(output_height);
            for (unsigned x = 0; x < output_width; ++x)
            {
                const float ndotv = (static_cast<float>(x) + 0.5f) / static_cast<float>(output_width);
                const glm::fvec2 integrated = IntegrateEnvironmentBrdf(ndotv, roughness);
                image.pixels[static_cast<std::size_t>(y) * output_width + x] =
                    glm::fvec4(integrated.x, integrated.y, 0.0f, 1.0f);
            }
        }

        return image;
    }

    RendererInterface::TextureDesc CreateTextureDescFromLinearImage(const LinearEnvironmentImage& image)
    {
        RendererInterface::TextureDesc texture_desc{};
        texture_desc.format = RendererInterface::RGBA8_UNORM;
        texture_desc.width = image.width;
        texture_desc.height = image.height;
        texture_desc.data.resize(static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) * 4u);
        for (std::size_t pixel_index = 0; pixel_index < image.pixels.size(); ++pixel_index)
        {
            const glm::fvec3 color = glm::clamp(image.pixels[pixel_index], glm::fvec3(0.0f), glm::fvec3(1.0f));
            const std::size_t base = pixel_index * 4u;
            texture_desc.data[base + 0u] = static_cast<char>(std::lround(color.r * 255.0f));
            texture_desc.data[base + 1u] = static_cast<char>(std::lround(color.g * 255.0f));
            texture_desc.data[base + 2u] = static_cast<char>(std::lround(color.b * 255.0f));
            texture_desc.data[base + 3u] = static_cast<char>(255);
        }
        return texture_desc;
    }

    RendererInterface::TextureDesc CreateTextureDescFromFloat4Image(
        const LinearFloat4Image& image,
        RendererInterface::PixelFormat format)
    {
        RendererInterface::TextureDesc texture_desc{};
        texture_desc.format = format;
        texture_desc.width = image.width;
        texture_desc.height = image.height;

        if (format == RendererInterface::RGBA16_FLOAT)
        {
            texture_desc.data.resize(
                static_cast<std::size_t>(image.width) *
                static_cast<std::size_t>(image.height) *
                sizeof(std::uint16_t) * 4u);
            for (std::size_t pixel_index = 0; pixel_index < image.pixels.size(); ++pixel_index)
            {
                const glm::fvec4 color = glm::max(image.pixels[pixel_index], glm::fvec4(0.0f));
                const std::uint16_t encoded_channels[4] = {
                    FloatToHalfBits(color.r),
                    FloatToHalfBits(color.g),
                    FloatToHalfBits(color.b),
                    FloatToHalfBits(color.a)
                };
                const std::size_t byte_offset = pixel_index * sizeof(encoded_channels);
                std::memcpy(texture_desc.data.data() + byte_offset, encoded_channels, sizeof(encoded_channels));
            }
            return texture_desc;
        }

        GLTF_CHECK(format == RendererInterface::RGBA8_UNORM);
        texture_desc.data.resize(static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) * 4u);
        for (std::size_t pixel_index = 0; pixel_index < image.pixels.size(); ++pixel_index)
        {
            const glm::fvec4 color = glm::clamp(image.pixels[pixel_index], glm::fvec4(0.0f), glm::fvec4(1.0f));
            const std::size_t base = pixel_index * 4u;
            texture_desc.data[base + 0u] = static_cast<char>(std::lround(color.r * 255.0f));
            texture_desc.data[base + 1u] = static_cast<char>(std::lround(color.g * 255.0f));
            texture_desc.data[base + 2u] = static_cast<char>(std::lround(color.b * 255.0f));
            texture_desc.data[base + 3u] = static_cast<char>(std::lround(color.a * 255.0f));
        }
        return texture_desc;
    }

    void NormalizeLightingGlobalParams(RendererSystemLighting::LightingGlobalParams& global_params)
    {
        const auto clamp = [](float value, float min_value, float max_value) -> float
        {
            return (std::max)(min_value, (std::min)(value, max_value));
        };

        global_params.environment_control.x = clamp(global_params.environment_control.x, 0.0f, 8.0f);
        global_params.environment_control.y = clamp(global_params.environment_control.y, 0.0f, 8.0f);
        global_params.environment_control.z = clamp(global_params.environment_control.z, 0.25f, 8.0f);
        global_params.environment_control.w = global_params.environment_control.w > 0.5f ? 1.0f : 0.0f;
        global_params.environment_texture_params.x = clamp(global_params.environment_texture_params.x, 0.0f, 8.0f);
        global_params.environment_texture_params.z = global_params.environment_texture_params.z > 0.5f ? 1.0f : 0.0f;
    }

    std::size_t ComputeLightTopologySignature(const std::vector<LightInfo>& lights)
    {
        std::size_t signature = 1469598103934665603ULL;
        auto hash_combine = [&signature](std::size_t value)
        {
            signature ^= value + 0x9e3779b97f4a7c15ULL + (signature << 6) + (signature >> 2);
        };

        hash_combine(lights.size());
        for (std::size_t i = 0; i < lights.size(); ++i)
        {
            hash_combine(i);
            hash_combine(static_cast<std::size_t>(lights[i].type));
        }

        return signature;
    }
}

void RendererSystemLighting::LightingPassRuntimeState::Reset()
{
    node = NULL_HANDLE;
    output = NULL_HANDLE;
    shadow_infos_handles.clear();
    light_topology_signature = 0;
}

bool RendererSystemLighting::LightingPassRuntimeState::HasInit() const
{
    return node != NULL_HANDLE;
}

void RendererSystemLighting::DirectionalShadowRuntimeState::Reset()
{
    m_resources.clear();
    m_fallback_shadow_maps.clear();
    m_bound_fallback_shadow_map = NULL_HANDLE;
}

bool RendererSystemLighting::DirectionalShadowRuntimeState::HasShadowPasses() const
{
    return !m_resources.empty();
}

size_t RendererSystemLighting::DirectionalShadowRuntimeState::GetShadowPassCount() const
{
    return m_resources.size();
}

void RendererSystemLighting::DirectionalShadowRuntimeState::CreateFallbackShadowMap(RendererInterface::ResourceOperator& resource_operator)
{
    if (!m_fallback_shadow_maps.empty())
    {
        return;
    }

    RendererInterface::RenderTargetDesc shadowmap_desc{};
    shadowmap_desc.name = "directional_shadowmap_fallback";
    shadowmap_desc.format = RendererInterface::D32;
    shadowmap_desc.width = 1;
    shadowmap_desc.height = 1;
    shadowmap_desc.clear = RendererInterface::default_clear_depth;
    shadowmap_desc.usage = static_cast<RendererInterface::ResourceUsage>(
        RendererInterface::ResourceUsage::DEPTH_STENCIL |
        RendererInterface::ResourceUsage::SHADER_RESOURCE);
    m_fallback_shadow_maps =
        resource_operator.CreateFrameBufferedRenderTargets(shadowmap_desc, shadowmap_desc.name);
    if (!m_fallback_shadow_maps.empty())
    {
        m_bound_fallback_shadow_map = m_fallback_shadow_maps.front();
    }
}

RendererSystemLighting::ShadowPassResource& RendererSystemLighting::DirectionalShadowRuntimeState::GetOrCreate(unsigned light_index)
{
    return m_resources[light_index];
}

std::map<unsigned, RendererSystemLighting::ShadowPassResource>& RendererSystemLighting::DirectionalShadowRuntimeState::GetResources()
{
    return m_resources;
}

const std::map<unsigned, RendererSystemLighting::ShadowPassResource>& RendererSystemLighting::DirectionalShadowRuntimeState::GetResources() const
{
    return m_resources;
}

bool RendererSystemLighting::DirectionalShadowRuntimeState::QueueRenderStateUpdates(
    RendererInterface::RenderGraph& graph,
    const RendererInterface::RenderStateDesc& render_state) const
{
    bool queued_all_shadow_passes = HasShadowPasses();
    for (const auto& shadow_resource_pair : m_resources)
    {
        if (!shadow_resource_pair.second.QueueRenderStateUpdate(graph, render_state))
        {
            queued_all_shadow_passes = false;
        }
    }

    return queued_all_shadow_passes;
}

void RendererSystemLighting::DirectionalShadowRuntimeState::SyncFallbackShadowMap(
    RendererInterface::ResourceOperator& resource_operator)
{
    if (!m_fallback_shadow_maps.empty())
    {
        m_bound_fallback_shadow_map = resource_operator.GetFrameBufferedRenderTargetHandle(m_fallback_shadow_maps);
    }
}

std::vector<RendererInterface::RenderTargetHandle> RendererSystemLighting::DirectionalShadowRuntimeState::SyncAndRegisterShadowPasses(
    RendererInterface::ResourceOperator& resource_operator,
    RendererInterface::RenderGraph& graph,
    const std::vector<LightInfo>& lights)
{
    SyncFallbackShadowMap(resource_operator);

    for (auto& shadow_resource_pair : m_resources)
    {
        auto& shadow_resource = shadow_resource_pair.second;
        shadow_resource.SyncFrameBindings(resource_operator, graph);
        shadow_resource.Register(graph);
    }

    std::vector<RendererInterface::RenderTargetHandle> current_shadow_maps;
    CollectLightIndexedShadowMaps(lights, current_shadow_maps);
    return current_shadow_maps;
}

void RendererSystemLighting::DirectionalShadowRuntimeState::CollectLightIndexedShadowMaps(
    const std::vector<LightInfo>& lights,
    std::vector<RendererInterface::RenderTargetHandle>& out_shadow_maps) const
{
    out_shadow_maps.clear();
    out_shadow_maps.resize(lights.size(), m_bound_fallback_shadow_map);
    for (unsigned light_index = 0; light_index < lights.size(); ++light_index)
    {
        if (lights[light_index].type != LightType::Directional)
        {
            continue;
        }

        const auto it = m_resources.find(light_index);
        if (it != m_resources.end() && it->second.m_bound_shadow_map != NULL_HANDLE)
        {
            out_shadow_maps[light_index] = it->second.m_bound_shadow_map;
        }
    }
}

void RendererSystemLighting::DirectionalShadowRuntimeState::CollectLightIndexedShadowMapInfos(
    const std::vector<LightInfo>& lights,
    std::vector<ShadowMapInfo>& out_shadowmap_infos) const
{
    out_shadowmap_infos.clear();
    out_shadowmap_infos.resize(lights.size());
    for (unsigned light_index = 0; light_index < lights.size(); ++light_index)
    {
        if (lights[light_index].type != LightType::Directional)
        {
            continue;
        }

        const auto it = m_resources.find(light_index);
        if (it != m_resources.end())
        {
            out_shadowmap_infos[light_index] = it->second.m_shadow_map_info;
        }
    }
}

void RendererSystemLighting::DirectionalShadowRuntimeState::CollectFallbackLightIndexedShadowMaps(
    const std::vector<LightInfo>& lights,
    std::vector<RendererInterface::RenderTargetHandle>& out_shadow_maps) const
{
    out_shadow_maps.clear();
    out_shadow_maps.resize(lights.size(), m_bound_fallback_shadow_map);
}

void RendererSystemLighting::DirectionalShadowRuntimeState::CollectDependencyNodes(
    std::vector<RendererInterface::RenderGraphNodeHandle>& out_dependency_nodes) const
{
    out_dependency_nodes.reserve(out_dependency_nodes.size() + m_resources.size());
    for (const auto& shadow_resource : m_resources)
    {
        if (shadow_resource.second.m_shadow_pass_node != NULL_HANDLE)
        {
            out_dependency_nodes.push_back(shadow_resource.second.m_shadow_pass_node);
        }
    }
}

bool RendererSystemLighting::ShadowPassResource::HasInit() const
{
    return m_shadow_pass_node != NULL_HANDLE;
}

bool RendererSystemLighting::ShadowPassResource::QueueRenderStateUpdate(
    RendererInterface::RenderGraph& graph,
    const RendererInterface::RenderStateDesc& render_state) const
{
    if (!HasInit())
    {
        return false;
    }

    return graph.QueueNodeRenderStateUpdate(m_shadow_pass_node, render_state);
}

RendererInterface::RenderTargetHandle RendererSystemLighting::ShadowPassResource::SyncFrameBindings(
    RendererInterface::ResourceOperator& resource_operator,
    RendererInterface::RenderGraph& graph)
{
    if (!m_shadow_maps.empty())
    {
        const auto current_shadow_map = resource_operator.GetFrameBufferedRenderTargetHandle(m_shadow_maps);
        if (m_bound_shadow_map == NULL_HANDLE)
        {
            m_bound_shadow_map = current_shadow_map;
        }
        else if (m_bound_shadow_map != current_shadow_map)
        {
            graph.UpdateNodeRenderTargetBinding(m_shadow_pass_node, m_bound_shadow_map, current_shadow_map);
            m_bound_shadow_map = current_shadow_map;
        }
    }

    if (!m_shadow_map_buffer_handles.empty())
    {
        graph.UpdateNodeBufferBinding(
            m_shadow_pass_node,
            "ViewBuffer",
            resource_operator.GetFrameBufferedBufferHandle(m_shadow_map_buffer_handles));
    }

    return m_bound_shadow_map;
}

void RendererSystemLighting::ShadowPassResource::Register(RendererInterface::RenderGraph& graph) const
{
    if (HasInit())
    {
        graph.RegisterRenderGraphNode(m_shadow_pass_node);
    }
}

RendererSystemLighting::RendererSystemLighting(RendererInterface::ResourceOperator& resource_operator,
                                               std::shared_ptr<RendererSystemSceneRenderer> scene,
                                               std::shared_ptr<RendererSystemSSAO> ssao)
    : m_scene(std::move(scene))
    , m_ssao(std::move(ssao))
    , m_directional_shadow_render_state(CreateDefaultDirectionalShadowRenderState())
{
    m_lighting_module = std::make_shared<RendererModuleLighting>(resource_operator);
    m_modules.push_back(m_lighting_module);
}

unsigned RendererSystemLighting::AddLight(const LightInfo& light_info)
{
    GLTF_CHECK(m_lighting_module);
    return m_lighting_module->AddLightInfo(light_info);
}

bool RendererSystemLighting::UpdateLight(unsigned index, const LightInfo& light_info)
{
    GLTF_CHECK(m_lighting_module);
    GLTF_CHECK(m_lighting_module->ContainsLight(index));
    return m_lighting_module->UpdateLightInfo(index, light_info);
}

bool RendererSystemLighting::GetDominantDirectionalLight(glm::fvec3& out_direction, float& out_luminance) const
{
    out_direction = {0.0f, -1.0f, 0.0f};
    out_luminance = 0.0f;
    if (!m_lighting_module)
    {
        return false;
    }

    const auto& lights = m_lighting_module->GetLightInfos();
    float strongest_luminance = -1.0f;
    bool found_directional_light = false;
    for (const auto& light_info : lights)
    {
        if (light_info.type != LightType::Directional)
        {
            continue;
        }

        const glm::fvec3 intensity = {
            (std::max)(0.0f, light_info.intensity.x),
            (std::max)(0.0f, light_info.intensity.y),
            (std::max)(0.0f, light_info.intensity.z)
        };
        const float luminance = glm::dot(intensity, glm::fvec3(0.2126f, 0.7152f, 0.0722f));
        if (!found_directional_light || luminance > strongest_luminance)
        {
            found_directional_light = true;
            strongest_luminance = luminance;
            out_direction = light_info.position;
        }
    }

    if (!found_directional_light)
    {
        return false;
    }

    const float direction_len_sq = glm::dot(out_direction, out_direction);
    if (direction_len_sq <= 1e-8f)
    {
        return false;
    }

    out_direction *= 1.0f / std::sqrt(direction_len_sq);
    out_luminance = (std::max)(0.0f, strongest_luminance);
    return true;
}

bool RendererSystemLighting::CastShadow() const
{
    return m_cast_shadow;
}

void RendererSystemLighting::SetCastShadow(bool cast_shadow)
{
    m_cast_shadow = cast_shadow;
}

const RendererInterface::RenderStateDesc& RendererSystemLighting::GetDirectionalShadowRenderState() const
{
    return m_directional_shadow_render_state;
}

bool RendererSystemLighting::SetDirectionalShadowRenderState(const RendererInterface::RenderStateDesc& render_state)
{
    const bool current_matches = IsEquivalentRenderStateDesc(m_directional_shadow_render_state, render_state);
    const bool pending_matches = m_pending_directional_shadow_render_state.has_value() &&
        IsEquivalentRenderStateDesc(*m_pending_directional_shadow_render_state, render_state);
    if (current_matches && (!m_pending_directional_shadow_render_state.has_value() || pending_matches))
    {
        return false;
    }

    m_directional_shadow_render_state = render_state;
    m_pending_directional_shadow_render_state = render_state;
    return true;
}

bool RendererSystemLighting::SetDirectionalShadowDepthBias(const RendererInterface::DepthBiasDesc& depth_bias)
{
    RendererInterface::RenderStateDesc render_state = m_directional_shadow_render_state;
    render_state.depth_bias = depth_bias;
    return SetDirectionalShadowRenderState(render_state);
}

RendererInterface::RenderTargetHandle RendererSystemLighting::GetLightingOutput() const
{
    return GetOutputs().output;
}

const RendererSystemLighting::LightingGlobalParams& RendererSystemLighting::GetGlobalParams() const
{
    return m_global_params;
}

void RendererSystemLighting::SetGlobalParams(const LightingGlobalParams& global_params)
{
    m_global_params = global_params;
    NormalizeLightingGlobalParams(m_global_params);
    m_need_upload_global_params = true;
}

RendererSystemLighting::LightingOutputs RendererSystemLighting::GetOutputs() const
{
    return LightingOutputs{
        .node = m_lighting_pass_state.node,
        .output = m_lighting_pass_state.output
    };
}

RendererInterface::RenderStateDesc RendererSystemLighting::CreateDefaultDirectionalShadowRenderState()
{
    RendererInterface::RenderStateDesc render_state{};
    render_state.depth_bias.enabled = true;
    render_state.depth_bias.constant_factor = 256.0f;
    render_state.depth_bias.slope_factor = 2.0f;
    return render_state;
}

bool RendererSystemLighting::Init(RendererInterface::ResourceOperator& resource_operator,
                                  RendererInterface::RenderGraph& graph)
{
    GLTF_CHECK(m_scene->HasInit());
    GLTF_CHECK(m_ssao);
    GLTF_CHECK(m_ssao->HasInit());
    CreateLightingOutput(resource_operator);

    RendererInterface::BufferDesc global_params_buffer_desc{};
    global_params_buffer_desc.name = "LightingGlobalBuffer";
    global_params_buffer_desc.size = sizeof(LightingGlobalParams);
    global_params_buffer_desc.type = RendererInterface::DEFAULT;
    global_params_buffer_desc.usage = RendererInterface::USAGE_CBV;
    m_lighting_global_params_handle = resource_operator.CreateBuffer(global_params_buffer_desc);
    m_global_params.environment_prefilter_roughness = glm::fvec4(
        ENVIRONMENT_PREFILTER_ROUGHNESS_LEVELS[0],
        ENVIRONMENT_PREFILTER_ROUGHNESS_LEVELS[1],
        ENVIRONMENT_PREFILTER_ROUGHNESS_LEVELS[2],
        ENVIRONMENT_PREFILTER_ROUGHNESS_LEVELS[3]);
    EnsureEnvironmentBrdfLut(resource_operator);
    EnsureEnvironmentTexture(resource_operator);

    m_directional_shadow_state.CreateFallbackShadowMap(resource_operator);

    const LightingExecutionPlan execution_plan = BuildLightingExecutionPlan();

    // Directional light shadow pass
    const auto& lights = m_lighting_module->GetLightInfos();
    for (unsigned i = 0; i < lights.size(); ++i)
    {
        const auto& light = lights[i];
        if (light.type != LightType::Directional)
        {
            continue;
        }

        CreateDirectionalShadowPassResource(resource_operator, graph, i, light);
    }

    CreateLightingPassShadowInfoBuffers(resource_operator);
    RETURN_IF_FALSE(RenderFeature::CreateRenderGraphNodeIfNeeded(
        resource_operator,
        graph,
        m_lighting_pass_state.node,
        BuildLightingPassSetupInfo(execution_plan)));
    m_lighting_pass_state.light_topology_signature = ComputeLightTopologySignature(lights);
    UploadGlobalParams(resource_operator);

    graph.RegisterRenderTargetToColorOutput(m_lighting_pass_state.output);
    
    return true;
}

RendererSystemLighting::LightingExecutionPlan RendererSystemLighting::BuildLightingExecutionPlan() const
{
    const auto camera_module = m_scene->GetCameraModule();
    GLTF_CHECK(camera_module);
    const unsigned extent_width = m_scene->GetWidth() > 0
        ? m_scene->GetWidth()
        : camera_module->GetWidth();
    const unsigned extent_height = m_scene->GetHeight() > 0
        ? m_scene->GetHeight()
        : camera_module->GetHeight();
    return LightingExecutionPlan{
        .camera_module = camera_module,
        .compute_plan = RenderFeature::ComputeExecutionPlan::FromExtent(extent_width, extent_height)
    };
}

bool RendererSystemLighting::HasInit() const
{
    return m_lighting_pass_state.HasInit();
}

void RendererSystemLighting::ResetRuntimeResources(RendererInterface::ResourceOperator& resource_operator)
{
    std::vector<LightInfo> cached_lights;
    if (m_lighting_module)
    {
        cached_lights = m_lighting_module->GetLightInfos();
    }

    m_lighting_module = std::make_shared<RendererModuleLighting>(resource_operator);
    for (const auto& light_info : cached_lights)
    {
        m_lighting_module->AddLightInfo(light_info);
    }

    m_modules.clear();
    m_modules.push_back(m_lighting_module);
    m_directional_shadow_state.Reset();
    m_lighting_pass_state.Reset();
    m_lighting_global_params_handle = NULL_HANDLE;
    m_environment_texture_handle = NULL_HANDLE;
    m_environment_irradiance_texture_handle = NULL_HANDLE;
    m_environment_prefilter_texture_handles.clear();
    m_environment_brdf_lut_texture_handle = NULL_HANDLE;
    m_need_upload_global_params = true;
}

bool RendererSystemLighting::Tick(RendererInterface::ResourceOperator& resource_operator,
                                  RendererInterface::RenderGraph& graph, unsigned long long interval)
{
    (void)interval;
    const LightingExecutionPlan execution_plan = BuildLightingExecutionPlan();
    const auto& lights = m_lighting_module->GetLightInfos();
    RETURN_IF_FALSE(SyncLightingTopology(resource_operator, graph, execution_plan));
    QueuePendingDirectionalShadowRenderStateUpdate(graph);
    execution_plan.compute_plan.ApplyDispatch(graph, m_lighting_pass_state.node);
    UploadGlobalParams(resource_operator);

    const auto& light_buffer_handles = m_lighting_module->GetLightBufferHandles();
    if (!light_buffer_handles.empty())
    {
        graph.UpdateNodeBufferBinding(
            m_lighting_pass_state.node,
            "g_lightInfos",
            resource_operator.GetFrameBufferedBufferHandle(light_buffer_handles));
    }
    const auto& light_count_buffer_handles = m_lighting_module->GetLightCountBufferHandles();
    if (!light_count_buffer_handles.empty())
    {
        graph.UpdateNodeBufferBinding(
            m_lighting_pass_state.node,
            "LightInfoConstantBuffer",
            resource_operator.GetFrameBufferedBufferHandle(light_count_buffer_handles));
    }
    if (!m_lighting_pass_state.shadow_infos_handles.empty())
    {
        graph.UpdateNodeBufferBinding(
            m_lighting_pass_state.node,
            "g_shadowmap_infos",
            resource_operator.GetFrameBufferedBufferHandle(m_lighting_pass_state.shadow_infos_handles));
    }

    std::vector<RendererInterface::RenderGraphNodeHandle> lighting_pass_dependencies;
    std::vector<RendererInterface::RenderTargetHandle> current_shadow_maps;
    if (CastShadow())
    {
        UpdateDirectionalShadowResources(resource_operator);
        current_shadow_maps = m_directional_shadow_state.SyncAndRegisterShadowPasses(
            resource_operator,
            graph,
            lights);
        m_directional_shadow_state.CollectDependencyNodes(lighting_pass_dependencies);
    }
    else
    {
        m_directional_shadow_state.SyncFallbackShadowMap(resource_operator);
        m_directional_shadow_state.CollectFallbackLightIndexedShadowMaps(lights, current_shadow_maps);
        if (!lights.empty() && !m_lighting_pass_state.shadow_infos_handles.empty())
        {
            const std::vector<ShadowMapInfo> disabled_shadow_infos(lights.size());
            RendererInterface::BufferUploadDesc shadow_info_upload_desc{};
            shadow_info_upload_desc.data = disabled_shadow_infos.data();
            shadow_info_upload_desc.size = disabled_shadow_infos.size() * sizeof(ShadowMapInfo);
            resource_operator.UploadFrameBufferedBufferData(m_lighting_pass_state.shadow_infos_handles, shadow_info_upload_desc);
        }
    }

    graph.UpdateNodeDependencies(m_lighting_pass_state.node, lighting_pass_dependencies);
    if (!current_shadow_maps.empty())
    {
        graph.UpdateNodeRenderTargetTextureBinding(
            m_lighting_pass_state.node,
            "bindless_shadowmap_textures",
            current_shadow_maps);
    }
    
    RETURN_IF_FALSE(RenderFeature::RegisterRenderGraphNodeIfValid(graph, m_lighting_pass_state.node));
    
    m_lighting_module->Tick(resource_operator, interval);
    
    return true;
}

void RendererSystemLighting::OnResize(RendererInterface::ResourceOperator& resource_operator, unsigned width, unsigned height)
{
    (void)resource_operator;
    (void)width;
    (void)height;
}

void RendererSystemLighting::DrawDebugUI()
{
    if (!m_lighting_module)
    {
        ImGui::TextUnformatted("Lighting module not initialized.");
        return;
    }

    bool cast_shadow = m_cast_shadow;
    if (ImGui::Checkbox("Cast Shadow", &cast_shadow))
    {
        SetCastShadow(cast_shadow);
    }

    bool global_dirty = false;
    bool environment_enabled = m_global_params.environment_control.w > 0.5f;
    if (ImGui::Checkbox("Enable Environment Lighting", &environment_enabled))
    {
        m_global_params.environment_control.w = environment_enabled ? 1.0f : 0.0f;
        global_dirty = true;
    }

    global_dirty |= ImGui::ColorEdit3("IBL Sky Zenith", &m_global_params.sky_zenith_color.x);
    global_dirty |= ImGui::ColorEdit3("IBL Sky Horizon", &m_global_params.sky_horizon_color.x);
    global_dirty |= ImGui::ColorEdit3("IBL Ground", &m_global_params.ground_color.x);
    global_dirty |= ImGui::SliderFloat("IBL Diffuse Intensity", &m_global_params.environment_control.x, 0.0f, 4.0f, "%.2f");
    global_dirty |= ImGui::SliderFloat("IBL Specular Intensity", &m_global_params.environment_control.y, 0.0f, 4.0f, "%.2f");
    global_dirty |= ImGui::SliderFloat("IBL Horizon Exponent", &m_global_params.environment_control.z, 0.25f, 4.0f, "%.2f");
    global_dirty |= ImGui::SliderFloat("Env Texture Intensity", &m_global_params.environment_texture_params.x, 0.0f, 4.0f, "%.2f");

    bool use_environment_texture = m_global_params.environment_texture_params.z > 0.5f;
    if (ImGui::Checkbox("Use Environment Texture", &use_environment_texture))
    {
        m_global_params.environment_texture_params.z = use_environment_texture ? 1.0f : 0.0f;
        global_dirty = true;
    }

    if (global_dirty)
    {
        NormalizeLightingGlobalParams(m_global_params);
        m_need_upload_global_params = true;
    }

    ImGui::Text("Lights: %u", static_cast<unsigned>(m_lighting_module->GetLightInfos().size()));
    ImGui::Text("Directional Shadow Maps: %u", static_cast<unsigned>(m_directional_shadow_state.GetShadowPassCount()));
    ImGui::Text("Environment Source: %s",
                m_global_params.environment_texture_params.z > 0.5f ? m_environment_texture_uri.c_str() : "Procedural gradient");
    ImGui::Text("Prefilter Levels: %.2f / %.2f / %.2f / %.2f",
                m_global_params.environment_prefilter_roughness.x,
                m_global_params.environment_prefilter_roughness.y,
                m_global_params.environment_prefilter_roughness.z,
                m_global_params.environment_prefilter_roughness.w);
}

bool RendererSystemLighting::QueuePendingDirectionalShadowRenderStateUpdate(RendererInterface::RenderGraph& graph)
{
    if (!m_pending_directional_shadow_render_state.has_value())
    {
        return true;
    }

    const bool queued_all_shadow_passes =
        m_directional_shadow_state.QueueRenderStateUpdates(graph, *m_pending_directional_shadow_render_state);

    if (queued_all_shadow_passes)
    {
        m_pending_directional_shadow_render_state.reset();
    }

    return queued_all_shadow_passes;
}

void RendererSystemLighting::CreateLightingOutput(RendererInterface::ResourceOperator& resource_operator)
{
    m_lighting_pass_state.output = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "LightingPass_Output",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        static_cast<RendererInterface::ResourceUsage>(
            RendererInterface::ResourceUsage::RENDER_TARGET |
            RendererInterface::ResourceUsage::COPY_SRC |
            RendererInterface::ResourceUsage::UNORDER_ACCESS |
            RendererInterface::ResourceUsage::SHADER_RESOURCE));
}

void RendererSystemLighting::EnsureEnvironmentTexture(RendererInterface::ResourceOperator& resource_operator)
{
    if (m_environment_texture_handle != NULL_HANDLE)
    {
        return;
    }

    LinearEnvironmentImage source_image{};
    const std::filesystem::path texture_path(m_environment_texture_uri);
    if (std::filesystem::exists(texture_path) && LoadLinearEnvironmentImage(m_environment_texture_uri, source_image))
    {
        m_environment_texture_handle = resource_operator.CreateTexture(CreateTextureDescFromLinearImage(source_image));
        m_global_params.environment_texture_params.z = 1.0f;
        m_need_upload_global_params = true;
        EnsureDerivedEnvironmentTextures(resource_operator);
        return;
    }

    RendererInterface::TextureDesc fallback_texture_desc{};
    fallback_texture_desc.format = RendererInterface::RGBA8_UNORM;
    fallback_texture_desc.width = 1u;
    fallback_texture_desc.height = 1u;
    fallback_texture_desc.data.assign(
        FALLBACK_ENVIRONMENT_TEXTURE_RGBA.begin(),
        FALLBACK_ENVIRONMENT_TEXTURE_RGBA.end());
    m_environment_texture_handle = resource_operator.CreateTexture(fallback_texture_desc);
    m_environment_irradiance_texture_handle = resource_operator.CreateTexture(fallback_texture_desc);
    m_environment_prefilter_texture_handles.assign(ENVIRONMENT_PREFILTER_ROUGHNESS_LEVELS.size(), NULL_HANDLE);
    for (auto& handle : m_environment_prefilter_texture_handles)
    {
        handle = resource_operator.CreateTexture(fallback_texture_desc);
    }
    m_global_params.environment_texture_params.z = 0.0f;
    m_need_upload_global_params = true;
}

void RendererSystemLighting::EnsureDerivedEnvironmentTextures(RendererInterface::ResourceOperator& resource_operator)
{
    const bool has_all_prefilter_levels =
        m_environment_prefilter_texture_handles.size() == ENVIRONMENT_PREFILTER_ROUGHNESS_LEVELS.size() &&
        std::all_of(
            m_environment_prefilter_texture_handles.begin(),
            m_environment_prefilter_texture_handles.end(),
            [](RendererInterface::TextureHandle handle)
            {
                return handle != NULL_HANDLE;
            });
    if (m_environment_irradiance_texture_handle != NULL_HANDLE && has_all_prefilter_levels)
    {
        return;
    }

    LinearEnvironmentImage source_image{};
    if (!LoadLinearEnvironmentImage(m_environment_texture_uri, source_image))
    {
        return;
    }

    const LinearEnvironmentImage irradiance_image =
        GenerateIrradianceEnvironment(source_image, ENVIRONMENT_IRRADIANCE_WIDTH, ENVIRONMENT_IRRADIANCE_HEIGHT);

    m_environment_irradiance_texture_handle = resource_operator.CreateTexture(CreateTextureDescFromLinearImage(irradiance_image));
    m_environment_prefilter_texture_handles.clear();
    m_environment_prefilter_texture_handles.reserve(ENVIRONMENT_PREFILTER_ROUGHNESS_LEVELS.size());
    for (std::size_t level_index = 0; level_index < ENVIRONMENT_PREFILTER_ROUGHNESS_LEVELS.size(); ++level_index)
    {
        const LinearEnvironmentImage prefilter_image =
            GeneratePrefilterEnvironment(source_image,
                                         ENVIRONMENT_PREFILTER_WIDTHS[level_index],
                                         ENVIRONMENT_PREFILTER_HEIGHTS[level_index],
                                         ENVIRONMENT_PREFILTER_ROUGHNESS_LEVELS[level_index]);
        m_environment_prefilter_texture_handles.push_back(
            resource_operator.CreateTexture(CreateTextureDescFromLinearImage(prefilter_image)));
    }
}

void RendererSystemLighting::EnsureEnvironmentBrdfLut(RendererInterface::ResourceOperator& resource_operator)
{
    if (m_environment_brdf_lut_texture_handle != NULL_HANDLE)
    {
        return;
    }

    const LinearFloat4Image brdf_lut =
        GenerateEnvironmentBrdfLut(ENVIRONMENT_BRDF_LUT_WIDTH, ENVIRONMENT_BRDF_LUT_HEIGHT);
    m_environment_brdf_lut_texture_handle = resource_operator.CreateTexture(
        CreateTextureDescFromFloat4Image(brdf_lut, RendererInterface::RGBA16_FLOAT));
}

void RendererSystemLighting::UploadGlobalParams(RendererInterface::ResourceOperator& resource_operator)
{
    if (!m_need_upload_global_params)
    {
        return;
    }

    RendererInterface::BufferUploadDesc global_params_upload_desc{};
    global_params_upload_desc.data = &m_global_params;
    global_params_upload_desc.size = sizeof(LightingGlobalParams);
    resource_operator.UploadBufferData(m_lighting_global_params_handle, global_params_upload_desc);
    m_need_upload_global_params = false;
}

void RendererSystemLighting::CreateLightingPassShadowInfoBuffers(RendererInterface::ResourceOperator& resource_operator)
{
    if (!m_lighting_pass_state.shadow_infos_handles.empty())
    {
        resource_operator.RetireFrameBufferedBuffers(m_lighting_pass_state.shadow_infos_handles);
        m_lighting_pass_state.shadow_infos_handles.clear();
    }

    std::vector<ShadowMapInfo> shadowmap_infos;
    m_directional_shadow_state.CollectLightIndexedShadowMapInfos(m_lighting_module->GetLightInfos(), shadowmap_infos);

    RendererInterface::BufferDesc shadowmap_info_buffer_desc{};
    shadowmap_info_buffer_desc.type = RendererInterface::DEFAULT;
    shadowmap_info_buffer_desc.name = "g_shadowmap_infos";
    shadowmap_info_buffer_desc.size = sizeof(ShadowMapInfo) * shadowmap_infos.size();
    shadowmap_info_buffer_desc.usage = RendererInterface::USAGE_SRV;
    shadowmap_info_buffer_desc.data = shadowmap_infos.data();
    m_lighting_pass_state.shadow_infos_handles =
        resource_operator.CreateFrameBufferedBuffers(shadowmap_info_buffer_desc, "g_shadowmap_infos");
}

bool RendererSystemLighting::SyncLightingTopology(
    RendererInterface::ResourceOperator& resource_operator,
    RendererInterface::RenderGraph& graph,
    const LightingExecutionPlan& execution_plan)
{
    const auto& lights = m_lighting_module->GetLightInfos();
    const std::size_t topology_signature = ComputeLightTopologySignature(lights);
    if (topology_signature == m_lighting_pass_state.light_topology_signature)
    {
        return true;
    }

    auto& shadow_resources = m_directional_shadow_state.GetResources();
    std::vector<unsigned> retired_shadow_light_indices;
    for (const auto& shadow_resource_pair : shadow_resources)
    {
        const unsigned light_index = shadow_resource_pair.first;
        if (light_index >= lights.size() || lights[light_index].type != LightType::Directional)
        {
            retired_shadow_light_indices.push_back(light_index);
        }
    }

    for (const unsigned light_index : retired_shadow_light_indices)
    {
        auto it = shadow_resources.find(light_index);
        if (it == shadow_resources.end())
        {
            continue;
        }

        if (it->second.m_shadow_pass_node != NULL_HANDLE)
        {
            graph.RemoveRenderGraphNode(it->second.m_shadow_pass_node);
        }
        resource_operator.RetireFrameBufferedRenderTargets(it->second.m_shadow_maps);
        resource_operator.RetireFrameBufferedBuffers(it->second.m_shadow_map_buffer_handles);
        shadow_resources.erase(it);
    }

    for (unsigned light_index = 0; light_index < lights.size(); ++light_index)
    {
        if (lights[light_index].type != LightType::Directional || shadow_resources.contains(light_index))
        {
            continue;
        }

        CreateDirectionalShadowPassResource(resource_operator, graph, light_index, lights[light_index]);
    }

    CreateLightingPassShadowInfoBuffers(resource_operator);
    RETURN_IF_FALSE(RenderFeature::SyncRenderGraphNode(
        resource_operator,
        graph,
        m_lighting_pass_state.node,
        BuildLightingPassSetupInfo(execution_plan)));
    m_lighting_pass_state.light_topology_signature = topology_signature;
    return true;
}

RendererSystemLighting::ShadowPassResource& RendererSystemLighting::CreateDirectionalShadowPassResource(
    RendererInterface::ResourceOperator& resource_operator,
    RendererInterface::RenderGraph& graph,
    unsigned light_index,
    const LightInfo& light_info)
{
    auto& shadow_pass_resource = m_directional_shadow_state.GetOrCreate(light_index);
    const std::string shadowmap_name = std::format("directional_shadowmap_{}", light_index);

    const unsigned shadowmap_width = 1024;
    const unsigned shadowmap_height = 1024;
    RendererInterface::RenderTargetDesc shadowmap_desc{};
    shadowmap_desc.name = shadowmap_name;
    shadowmap_desc.format = RendererInterface::D32;
    shadowmap_desc.width = shadowmap_width;
    shadowmap_desc.height = shadowmap_height;
    shadowmap_desc.clear = RendererInterface::default_clear_depth;
    shadowmap_desc.usage = static_cast<RendererInterface::ResourceUsage>(
        RendererInterface::ResourceUsage::DEPTH_STENCIL |
        RendererInterface::ResourceUsage::SHADER_RESOURCE);
    shadow_pass_resource.m_shadow_maps =
        resource_operator.CreateFrameBufferedRenderTargets(shadowmap_desc, shadowmap_name);
    GLTF_CHECK(!shadow_pass_resource.m_shadow_maps.empty());
    shadow_pass_resource.m_bound_shadow_map = shadow_pass_resource.m_shadow_maps[0];

    ShadowPassResource::CalcDirectionalLightShadowMatrix(
        light_info,
        m_scene->GetSceneMeshModule()->GetSceneBounds(),
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        shadowmap_width,
        shadowmap_height,
        shadow_pass_resource.m_shadow_map_view_buffer,
        shadow_pass_resource.m_shadow_map_info);
    RendererInterface::BufferDesc camera_buffer_desc{};
    camera_buffer_desc.name = "ViewBuffer";
    camera_buffer_desc.size = sizeof(ViewBuffer);
    camera_buffer_desc.type = RendererInterface::DEFAULT;
    camera_buffer_desc.usage = RendererInterface::USAGE_CBV;
    camera_buffer_desc.data = &shadow_pass_resource.m_shadow_map_view_buffer;
    shadow_pass_resource.m_shadow_map_buffer_handles =
        resource_operator.CreateFrameBufferedBuffers(
            camera_buffer_desc,
            std::format("ViewBuffer_shadow_{}", light_index));
    shadow_pass_resource.m_shadow_map_view_buffers.assign(
        shadow_pass_resource.m_shadow_map_buffer_handles.size(),
        shadow_pass_resource.m_shadow_map_view_buffer);

    shadow_pass_resource.m_shadow_pass_node = graph.CreateRenderGraphNode(
        resource_operator,
        BuildDirectionalShadowPassSetupInfo(shadow_pass_resource, light_index));
    GLTF_CHECK(!shadow_pass_resource.m_shadow_map_buffer_handles.empty());
    return shadow_pass_resource;
}

RendererInterface::RenderGraph::RenderPassSetupInfo RendererSystemLighting::BuildDirectionalShadowPassSetupInfo(
    const ShadowPassResource& shadow_pass_resource,
    unsigned light_index) const
{
    return RenderFeature::PassBuilder::Graphics("Lighting", std::format("Directional Shadow {}", light_index))
        .SetRenderState(m_directional_shadow_render_state)
        .SetViewport(
            static_cast<int>(shadow_pass_resource.m_shadow_map_info.shadowmap_size[0]),
            static_cast<int>(shadow_pass_resource.m_shadow_map_info.shadowmap_size[1]))
        .AddModule(m_scene->GetSceneMeshModule())
        .AddShader(
            RendererInterface::ShaderType::VERTEX_SHADER,
            "MainVS",
            "Resources/Shaders/ModelRenderingShader.hlsl")
        .AddRenderTargets({
            RenderFeature::MakeRenderTargetAttachment(
                shadow_pass_resource.m_bound_shadow_map,
                RenderFeature::MakeDepthRenderTargetBinding(RendererInterface::D32, true))
        })
        .AddBuffers({
            RenderFeature::MakeBufferBinding(
                "ViewBuffer",
                RenderFeature::MakeConstantBufferBinding(shadow_pass_resource.m_shadow_map_buffer_handles[0]))
        })
        .ExcludeBufferBinding("g_material_infos")
        .ExcludeTextureBinding("bindless_material_textures")
        .Build();
}

RendererInterface::RenderGraph::RenderPassSetupInfo RendererSystemLighting::BuildLightingPassSetupInfo(
    const LightingExecutionPlan& execution_plan) const
{
    const auto scene_outputs = m_scene->GetOutputs();
    const auto ssao_outputs = m_ssao->GetOutputs();
    std::vector<RendererInterface::RenderTargetHandle> initial_shadow_maps;
    m_directional_shadow_state.CollectLightIndexedShadowMaps(m_lighting_module->GetLightInfos(), initial_shadow_maps);
    GLTF_CHECK(!m_lighting_pass_state.shadow_infos_handles.empty());
    GLTF_CHECK(ssao_outputs.output != NULL_HANDLE);
    GLTF_CHECK(m_environment_texture_handle != NULL_HANDLE);
    GLTF_CHECK(m_environment_irradiance_texture_handle != NULL_HANDLE);
    GLTF_CHECK(m_environment_prefilter_texture_handles.size() == ENVIRONMENT_PREFILTER_ROUGHNESS_LEVELS.size());
    for (const auto handle : m_environment_prefilter_texture_handles)
    {
        GLTF_CHECK(handle != NULL_HANDLE);
    }
    GLTF_CHECK(m_environment_brdf_lut_texture_handle != NULL_HANDLE);
    auto builder = RenderFeature::PassBuilder::Compute("Lighting", "Scene Lighting");
    RendererInterface::TextureBindingDesc environment_texture_binding{};
    environment_texture_binding.type = RendererInterface::TextureBindingDesc::SRV;
    environment_texture_binding.textures.push_back(m_environment_texture_handle);
    RendererInterface::TextureBindingDesc environment_irradiance_binding{};
    environment_irradiance_binding.type = RendererInterface::TextureBindingDesc::SRV;
    environment_irradiance_binding.textures.push_back(m_environment_irradiance_texture_handle);
    RendererInterface::TextureBindingDesc environment_brdf_lut_binding{};
    environment_brdf_lut_binding.type = RendererInterface::TextureBindingDesc::SRV;
    environment_brdf_lut_binding.textures.push_back(m_environment_brdf_lut_texture_handle);
    builder.AddModules({m_lighting_module, execution_plan.camera_module})
        .AddShader(RendererInterface::COMPUTE_SHADER, "main", "Resources/Shaders/SceneLighting.hlsl")
        .AddSampledRenderTargetBindings({
            RenderFeature::MakeSampledRenderTargetBinding(
                "albedoTex",
                scene_outputs.color,
                RendererInterface::RenderTargetTextureBindingDesc::SRV),
            RenderFeature::MakeSampledRenderTargetBinding(
                "normalTex",
                scene_outputs.normal,
                RendererInterface::RenderTargetTextureBindingDesc::SRV),
            RenderFeature::MakeSampledRenderTargetBinding(
                "depthTex",
                scene_outputs.depth,
                RendererInterface::RenderTargetTextureBindingDesc::SRV),
            RenderFeature::MakeSampledRenderTargetBinding(
                "ssaoTex",
                ssao_outputs.output,
                RendererInterface::RenderTargetTextureBindingDesc::SRV),
            RenderFeature::MakeSampledRenderTargetBinding(
                "bindless_shadowmap_textures",
                initial_shadow_maps,
                RendererInterface::RenderTargetTextureBindingDesc::SRV),
            RenderFeature::MakeSampledRenderTargetBinding(
                "Output",
                m_lighting_pass_state.output,
                RendererInterface::RenderTargetTextureBindingDesc::UAV)
        })
        .AddTexture("environmentTex", environment_texture_binding)
        .AddTexture("environmentIrradianceTex", environment_irradiance_binding)
        .AddTexture("environmentBrdfLutTex", environment_brdf_lut_binding)
        .AddBuffers({
            RenderFeature::MakeBufferBinding(
                "g_shadowmap_infos",
                RenderFeature::MakeStructuredBufferBinding(
                    m_lighting_pass_state.shadow_infos_handles.front(),
                    RendererInterface::BufferBindingDesc::SRV,
                    sizeof(ShadowMapInfo),
                    static_cast<unsigned>(m_lighting_module->GetLightInfos().size()))),
            RenderFeature::MakeBufferBinding(
                "LightingGlobalBuffer",
                RenderFeature::MakeConstantBufferBinding(m_lighting_global_params_handle))
        })
        .SetDispatch(execution_plan.compute_plan);

    for (std::size_t level_index = 0; level_index < m_environment_prefilter_texture_handles.size(); ++level_index)
    {
        RendererInterface::TextureBindingDesc environment_prefilter_binding{};
        environment_prefilter_binding.type = RendererInterface::TextureBindingDesc::SRV;
        environment_prefilter_binding.textures.push_back(m_environment_prefilter_texture_handles[level_index]);
        builder.AddTexture(std::format("environmentPrefilterTex{}", level_index), environment_prefilter_binding);
    }

    std::vector<RendererInterface::RenderGraphNodeHandle> dependency_nodes;
    m_directional_shadow_state.CollectDependencyNodes(dependency_nodes);
    dependency_nodes.push_back(ssao_outputs.blur_node);
    builder.AddDependencies(dependency_nodes);

    return builder.Build();
}

void RendererSystemLighting::UpdateDirectionalShadowResources(RendererInterface::ResourceOperator& resource_operator)
{
    if (!m_directional_shadow_state.HasShadowPasses())
    {
        return;
    }

    std::vector<ShadowMapInfo> shadowmap_infos;
    const unsigned current_frame_slot_index = resource_operator.GetCurrentFrameSlotIndex();

    const auto& lights = m_lighting_module->GetLightInfos();
    const auto scene_bounds = m_scene->GetSceneMeshModule()->GetSceneBounds();
    for (auto& shadow_resource_pair : m_directional_shadow_state.GetResources())
    {
        const unsigned light_index = shadow_resource_pair.first;
        auto& shadow_resource = shadow_resource_pair.second;
        GLTF_CHECK(light_index < lights.size());

        const auto& light_info = lights[light_index];
        GLTF_CHECK(light_info.type == LightType::Directional);
        const unsigned shadowmap_width = shadow_resource.m_shadow_map_info.shadowmap_size[0];
        const unsigned shadowmap_height = shadow_resource.m_shadow_map_info.shadowmap_size[1];
        ShadowPassResource::CalcDirectionalLightShadowMatrix(
            light_info,
            scene_bounds,
            0.0f,
            0.0f,
            1.0f,
            1.0f,
            shadowmap_width,
            shadowmap_height,
            shadow_resource.m_shadow_map_view_buffer,
            shadow_resource.m_shadow_map_info);

        if (!shadow_resource.m_shadow_map_buffer_handles.empty() &&
            shadow_resource.m_shadow_map_buffer_handles.size() == shadow_resource.m_shadow_map_view_buffers.size())
        {
            const unsigned frame_slot = current_frame_slot_index % static_cast<unsigned>(shadow_resource.m_shadow_map_buffer_handles.size());
            shadow_resource.m_shadow_map_view_buffers[frame_slot] = shadow_resource.m_shadow_map_view_buffer;

            RendererInterface::BufferUploadDesc shadow_camera_upload_desc{};
            shadow_camera_upload_desc.data = &shadow_resource.m_shadow_map_view_buffers[frame_slot];
            shadow_camera_upload_desc.size = sizeof(ViewBuffer);
            resource_operator.UploadFrameBufferedBufferData(shadow_resource.m_shadow_map_buffer_handles, shadow_camera_upload_desc);
        }
    }
    m_directional_shadow_state.CollectLightIndexedShadowMapInfos(lights, shadowmap_infos);

    if (!shadowmap_infos.empty() && !m_lighting_pass_state.shadow_infos_handles.empty())
    {
        RendererInterface::BufferUploadDesc shadow_info_upload_desc{};
        shadow_info_upload_desc.data = shadowmap_infos.data();
        shadow_info_upload_desc.size = shadowmap_infos.size() * sizeof(ShadowMapInfo);
        resource_operator.UploadFrameBufferedBufferData(m_lighting_pass_state.shadow_infos_handles, shadow_info_upload_desc);
    }
}

bool RendererSystemLighting::ShadowPassResource::CalcDirectionalLightShadowMatrix(
    const LightInfo& directional_light_info, const RendererSceneAABB& scene_bounds, float ndc_min_x, float ndc_min_y,
    float ndc_width, float ndc_height, unsigned shadowmap_width, unsigned shadowmap_height,
    ViewBuffer& out_view_buffer, ShadowMapInfo& out_shadow_info)
{
    const auto& scene_bounds_min = scene_bounds.getMin();
    const auto& scene_bounds_max = scene_bounds.getMax();
    const auto& scene_center = scene_bounds.getCenter();
    
    glm::vec3 light_dir = directional_light_info.position;
    const float light_len_sq = glm::dot(light_dir, light_dir);
    if (light_len_sq <= 1e-8f)
    {
        return false;
    }
    light_dir *= 1.0f / std::sqrt(light_len_sq);

    const glm::vec3 scene_extent = scene_bounds_max - scene_bounds_min;
    const float scene_radius = (std::max)(0.5f * glm::length(scene_extent), 1.0f);
    const float shadow_radius = scene_radius * 1.1f;

    glm::vec3 light_up = {0.0f, 1.0f, 0.0f};
    if (std::abs(glm::dot(light_dir, light_up)) > 0.99f)
    {
        light_up = {0.0f, 0.0f, 1.0f};
    }

    const glm::vec3 light_eye = scene_center - light_dir * shadow_radius;
    glm::mat4x4 view_matrix = glm::lookAtLH(light_eye, scene_center, light_up);

    std::array<glm::vec4, 8> scene_corners = {
        glm::vec4(scene_bounds_min.x, scene_bounds_min.y, scene_bounds_min.z, 1.0f),
        glm::vec4(scene_bounds_max.x, scene_bounds_min.y, scene_bounds_min.z, 1.0f),
        glm::vec4(scene_bounds_min.x, scene_bounds_max.y, scene_bounds_min.z, 1.0f),
        glm::vec4(scene_bounds_max.x, scene_bounds_max.y, scene_bounds_min.z, 1.0f),
        glm::vec4(scene_bounds_min.x, scene_bounds_min.y, scene_bounds_max.z, 1.0f),
        glm::vec4(scene_bounds_max.x, scene_bounds_min.y, scene_bounds_max.z, 1.0f),
        glm::vec4(scene_bounds_min.x, scene_bounds_max.y, scene_bounds_max.z, 1.0f),
        glm::vec4(scene_bounds_max.x, scene_bounds_max.y, scene_bounds_max.z, 1.0f)};

    glm::vec3 light_space_min{
        (std::numeric_limits<float>::max)(),
        (std::numeric_limits<float>::max)(),
        (std::numeric_limits<float>::max)()};
    glm::vec3 light_space_max{
        (std::numeric_limits<float>::lowest)(),
        (std::numeric_limits<float>::lowest)(),
        (std::numeric_limits<float>::lowest)()};
    for (const auto& scene_corner : scene_corners)
    {
        const glm::vec4 light_space_corner = view_matrix * scene_corner;
        light_space_min = glm::min(light_space_min, glm::vec3(light_space_corner));
        light_space_max = glm::max(light_space_max, glm::vec3(light_space_corner));
    }

    const glm::vec3 light_space_extent = light_space_max - light_space_min;
    const glm::vec3 light_space_padding = glm::max(light_space_extent * 0.01f, glm::vec3(0.5f, 0.5f, 1.0f));
    light_space_min -= light_space_padding;
    light_space_max += light_space_padding;

    const float ndc_total_width = (std::max)(light_space_max.x - light_space_min.x, 1.0f);
    const float ndc_total_height = (std::max)(light_space_max.y - light_space_min.y, 1.0f);
    float new_ndc_min_x = ndc_min_x * ndc_total_width + light_space_min.x;
    float new_ndc_min_y = ndc_min_y * ndc_total_height + light_space_min.y;
    float new_ndc_max_x = new_ndc_min_x + ndc_width * ndc_total_width;
    float new_ndc_max_y = new_ndc_min_y + ndc_height * ndc_total_height;

    const float shadowmap_width_safe = static_cast<float>((std::max)(1u, shadowmap_width));
    const float shadowmap_height_safe = static_cast<float>((std::max)(1u, shadowmap_height));
    const float texel_size_x = (new_ndc_max_x - new_ndc_min_x) / shadowmap_width_safe;
    const float texel_size_y = (new_ndc_max_y - new_ndc_min_y) / shadowmap_height_safe;
    if (texel_size_x > 0.0f && texel_size_y > 0.0f)
    {
        const float extent_x = 0.5f * (new_ndc_max_x - new_ndc_min_x);
        const float extent_y = 0.5f * (new_ndc_max_y - new_ndc_min_y);
        float center_x = 0.5f * (new_ndc_min_x + new_ndc_max_x);
        float center_y = 0.5f * (new_ndc_min_y + new_ndc_max_y);
        center_x = std::floor(center_x / texel_size_x) * texel_size_x;
        center_y = std::floor(center_y / texel_size_y) * texel_size_y;
        new_ndc_min_x = center_x - extent_x;
        new_ndc_max_x = center_x + extent_x;
        new_ndc_min_y = center_y - extent_y;
        new_ndc_max_y = center_y + extent_y;
    }

    const float near_plane = light_space_min.z;
    const float far_plane = (std::max)(light_space_max.z, near_plane + 1.0f);
    glm::mat4x4 projection_matrix = glm::orthoLH(
        new_ndc_min_x,
        new_ndc_max_x,
        new_ndc_min_y,
        new_ndc_max_y,
        near_plane,
        far_plane);
    
    out_view_buffer.viewport_width = shadowmap_width;
    out_view_buffer.viewport_height = shadowmap_height;
    out_view_buffer.view_position = {light_eye, 1.0f};
    out_view_buffer.view_projection_matrix = projection_matrix * view_matrix;
    out_view_buffer.prev_view_projection_matrix = out_view_buffer.view_projection_matrix;
    out_view_buffer.inverse_view_projection_matrix = glm::inverse(out_view_buffer.view_projection_matrix);

    out_shadow_info.view_matrix = view_matrix;
    out_shadow_info.projection_matrix = projection_matrix;
    out_shadow_info.shadowmap_size[0] = shadowmap_width;
    out_shadow_info.shadowmap_size[1] = shadowmap_height;
    out_shadow_info.vsm_texture_id = -1;
    
    return true;
}
