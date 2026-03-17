// DX use [0, 1] as depth clip range
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "EnvironmentLightingResources.h"

#include "SceneFileLoader/glTFImageIOUtil.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>

namespace
{
    constexpr float CPU_PI = 3.14159265358979323846f;
    constexpr unsigned ENVIRONMENT_IRRADIANCE_WIDTH = 64u;
    constexpr unsigned ENVIRONMENT_IRRADIANCE_HEIGHT = 32u;
    constexpr unsigned ENVIRONMENT_BRDF_LUT_WIDTH = 128u;
    constexpr unsigned ENVIRONMENT_BRDF_LUT_HEIGHT = 128u;
    constexpr std::array<unsigned, EnvironmentLightingResources::kPrefilterLevelCount> ENVIRONMENT_PREFILTER_WIDTHS = {
        128u, 64u, 32u, 16u};
    constexpr std::array<unsigned, EnvironmentLightingResources::kPrefilterLevelCount> ENVIRONMENT_PREFILTER_HEIGHTS = {
        64u, 32u, 16u, 8u};
    constexpr std::array<float, EnvironmentLightingResources::kPrefilterLevelCount> ENVIRONMENT_PREFILTER_ROUGHNESS_LEVELS = {
        0.25f, 0.50f, 0.75f, 1.00f};
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
            const std::size_t clamped_y =
                static_cast<std::size_t>((std::max)(0, (std::min)(sample_y, static_cast<int>(image.height) - 1)));
            return image.pixels[clamped_y * image.width + wrapped_x];
        };

        const glm::fvec3 c00 = sample_pixel(x0, y0);
        const glm::fvec3 c10 = sample_pixel(x1, y0);
        const glm::fvec3 c01 = sample_pixel(x0, y1);
        const glm::fvec3 c11 = sample_pixel(x1, y1);
        const glm::fvec3 c0 = glm::mix(c00, c10, tx);
        const glm::fvec3 c1 = glm::mix(c01, c11, tx);
        return glm::mix(c0, c1, ty);
    }

    glm::fvec3 DirectionFromLatLong(float u, float v)
    {
        const float phi = (u - 0.5f) * 2.0f * CPU_PI;
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

    glm::fvec3 TransformLocalToWorld(const glm::fvec3& local_dir, const glm::fvec3& normal)
    {
        glm::fvec3 tangent{};
        glm::fvec3 bitangent{};
        BuildOrthonormalBasis(normal, tangent, bitangent);
        return glm::normalize(tangent * local_dir.x + normal * local_dir.y + bitangent * local_dir.z);
    }

    glm::fvec2 Hammersley2D(unsigned sample_index, unsigned sample_count)
    {
        unsigned bits = sample_index;
        bits = (bits << 16u) | (bits >> 16u);
        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
        const float radical_inverse = static_cast<float>(bits) * 2.3283064365386963e-10f;
        return glm::fvec2(
            static_cast<float>(sample_index) / static_cast<float>((std::max)(1u, sample_count)),
            radical_inverse);
    }

    glm::fvec3 SampleCosineHemisphere(unsigned sample_index, unsigned sample_count)
    {
        const glm::fvec2 xi = Hammersley2D(sample_index, sample_count);
        const float phi = 2.0f * CPU_PI * xi.x;
        const float cos_theta = std::sqrt((std::max)(0.0f, 1.0f - xi.y));
        const float sin_theta = std::sqrt((std::max)(0.0f, xi.y));
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

            mantissa |= 0x00800000u;
            const std::uint32_t shifted_mantissa = mantissa >> static_cast<unsigned>(1 - half_exponent);
            const std::uint16_t rounded = static_cast<std::uint16_t>((shifted_mantissa + 0x00001000u) >> 13u);
            return static_cast<std::uint16_t>(sign | rounded);
        }

        const std::uint16_t half = static_cast<std::uint16_t>(
            sign |
            (static_cast<std::uint32_t>(half_exponent) << 10u) |
            ((mantissa + 0x00001000u) >> 13u));
        return half;
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
        const float cos_theta =
            std::sqrt((std::max)(0.0f, (1.0f - xi.y) / (1.0f + (alpha * alpha - 1.0f) * xi.y)));
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

    LinearEnvironmentImage GenerateIrradianceEnvironment(
        const LinearEnvironmentImage& source,
        unsigned output_width,
        unsigned output_height)
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
                    const glm::fvec3 local_dir =
                        SampleCosineHemisphere(sample_index, ENVIRONMENT_IRRADIANCE_SAMPLE_COUNT);
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
        const float cone_cos_theta_max = std::cos(roughness * roughness * 0.5f * CPU_PI);
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
                    const glm::fvec3 local_dir =
                        SampleCone(sample_index, ENVIRONMENT_PREFILTER_SAMPLE_COUNT, cone_cos_theta_max);
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

    RendererInterface::TextureMipLevelDesc CreateTextureMipLevelDescFromLinearImage(const LinearEnvironmentImage& image)
    {
        RendererInterface::TextureMipLevelDesc mip_desc{};
        mip_desc.width = image.width;
        mip_desc.height = image.height;
        mip_desc.data.resize(static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) * 4u);
        for (std::size_t pixel_index = 0; pixel_index < image.pixels.size(); ++pixel_index)
        {
            const glm::fvec3 color = glm::clamp(image.pixels[pixel_index], glm::fvec3(0.0f), glm::fvec3(1.0f));
            const std::size_t base = pixel_index * 4u;
            mip_desc.data[base + 0u] = static_cast<char>(std::lround(color.r * 255.0f));
            mip_desc.data[base + 1u] = static_cast<char>(std::lround(color.g * 255.0f));
            mip_desc.data[base + 2u] = static_cast<char>(std::lround(color.b * 255.0f));
            mip_desc.data[base + 3u] = static_cast<char>(255);
        }
        return mip_desc;
    }

    RendererInterface::TextureDesc CreateTextureDescFromLinearImage(const LinearEnvironmentImage& image)
    {
        RendererInterface::TextureDesc texture_desc{};
        texture_desc.format = RendererInterface::RGBA8_UNORM;
        texture_desc.width = image.width;
        texture_desc.height = image.height;
        texture_desc.data = CreateTextureMipLevelDescFromLinearImage(image).data;
        return texture_desc;
    }

    RendererInterface::TextureDesc CreateTextureDescFromLinearImageMipChain(
        const std::vector<LinearEnvironmentImage>& mip_images)
    {
        GLTF_CHECK(!mip_images.empty());
        RendererInterface::TextureDesc texture_desc{};
        texture_desc.format = RendererInterface::RGBA8_UNORM;
        texture_desc.width = mip_images.front().width;
        texture_desc.height = mip_images.front().height;
        texture_desc.generate_mipmaps = true;
        texture_desc.mip_levels.reserve(mip_images.size());
        for (const auto& mip_image : mip_images)
        {
            texture_desc.mip_levels.push_back(CreateTextureMipLevelDescFromLinearImage(mip_image));
        }
        return texture_desc;
    }

    RendererInterface::TextureMipLevelDesc CreateSolidColorMipLevelDesc(
        unsigned width,
        unsigned height,
        const std::array<char, 4>& rgba)
    {
        RendererInterface::TextureMipLevelDesc mip_desc{};
        mip_desc.width = width;
        mip_desc.height = height;
        mip_desc.data.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
        for (std::size_t pixel_index = 0; pixel_index < static_cast<std::size_t>(width) * static_cast<std::size_t>(height); ++pixel_index)
        {
            const std::size_t base = pixel_index * 4u;
            mip_desc.data[base + 0u] = rgba[0];
            mip_desc.data[base + 1u] = rgba[1];
            mip_desc.data[base + 2u] = rgba[2];
            mip_desc.data[base + 3u] = rgba[3];
        }
        return mip_desc;
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
}

bool EnvironmentLightingResources::Init(RendererInterface::ResourceOperator& resource_operator)
{
    EnsureEnvironmentBrdfLut(resource_operator);
    EnsureEnvironmentTextureSet(resource_operator);
    return IsReady();
}

void EnvironmentLightingResources::Reset()
{
    m_environment_texture_handle = NULL_HANDLE;
    m_environment_irradiance_texture_handle = NULL_HANDLE;
    m_environment_prefilter_texture_handle = NULL_HANDLE;
    m_environment_brdf_lut_texture_handle = NULL_HANDLE;
    m_has_texture_source = false;
}

bool EnvironmentLightingResources::IsReady() const
{
    return m_environment_texture_handle != NULL_HANDLE &&
           m_environment_irradiance_texture_handle != NULL_HANDLE &&
           HasPrefilterTexture() &&
           m_environment_brdf_lut_texture_handle != NULL_HANDLE;
}

bool EnvironmentLightingResources::HasTextureSource() const
{
    return m_has_texture_source;
}

const std::string& EnvironmentLightingResources::GetSourceTextureUri() const
{
    return m_environment_texture_uri;
}

glm::fvec4 EnvironmentLightingResources::GetPrefilterRoughnessParams() const
{
    return glm::fvec4(
        ENVIRONMENT_PREFILTER_ROUGHNESS_LEVELS[0],
        ENVIRONMENT_PREFILTER_ROUGHNESS_LEVELS[1],
        ENVIRONMENT_PREFILTER_ROUGHNESS_LEVELS[2],
        ENVIRONMENT_PREFILTER_ROUGHNESS_LEVELS[3]);
}

const std::array<float, EnvironmentLightingResources::kPrefilterLevelCount>&
EnvironmentLightingResources::GetPrefilterRoughnessLevels() const
{
    return ENVIRONMENT_PREFILTER_ROUGHNESS_LEVELS;
}

void EnvironmentLightingResources::AddLightingPassBindings(RenderFeature::PassBuilder& builder) const
{
    GLTF_CHECK(IsReady());

    RendererInterface::TextureBindingDesc environment_texture_binding{};
    environment_texture_binding.type = RendererInterface::TextureBindingDesc::SRV;
    environment_texture_binding.textures.push_back(m_environment_texture_handle);

    RendererInterface::TextureBindingDesc environment_irradiance_binding{};
    environment_irradiance_binding.type = RendererInterface::TextureBindingDesc::SRV;
    environment_irradiance_binding.textures.push_back(m_environment_irradiance_texture_handle);

    RendererInterface::TextureBindingDesc environment_brdf_lut_binding{};
    environment_brdf_lut_binding.type = RendererInterface::TextureBindingDesc::SRV;
    environment_brdf_lut_binding.textures.push_back(m_environment_brdf_lut_texture_handle);

    builder.AddTexture("environmentTex", environment_texture_binding)
        .AddTexture("environmentIrradianceTex", environment_irradiance_binding)
        .AddTexture("environmentBrdfLutTex", environment_brdf_lut_binding);

    RendererInterface::TextureBindingDesc environment_prefilter_binding{};
    environment_prefilter_binding.type = RendererInterface::TextureBindingDesc::SRV;
    environment_prefilter_binding.textures.push_back(m_environment_prefilter_texture_handle);
    builder.AddTexture("environmentPrefilterTex", environment_prefilter_binding);
}

bool EnvironmentLightingResources::HasPrefilterTexture() const
{
    return m_environment_prefilter_texture_handle != NULL_HANDLE;
}

void EnvironmentLightingResources::EnsureEnvironmentTextureSet(RendererInterface::ResourceOperator& resource_operator)
{
    if (m_environment_texture_handle != NULL_HANDLE &&
        m_environment_irradiance_texture_handle != NULL_HANDLE &&
        HasPrefilterTexture())
    {
        return;
    }

    BuildTexturesFromSource(resource_operator);
}

void EnvironmentLightingResources::EnsureEnvironmentBrdfLut(RendererInterface::ResourceOperator& resource_operator)
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

void EnvironmentLightingResources::BuildTexturesFromSource(RendererInterface::ResourceOperator& resource_operator)
{
    LinearEnvironmentImage source_image{};
    const std::filesystem::path texture_path(m_environment_texture_uri);
    if (!std::filesystem::exists(texture_path) || !LoadLinearEnvironmentImage(m_environment_texture_uri, source_image))
    {
        CreateFallbackTextureSet(resource_operator);
        return;
    }

    m_environment_texture_handle = resource_operator.CreateTexture(CreateTextureDescFromLinearImage(source_image));

    const LinearEnvironmentImage irradiance_image =
        GenerateIrradianceEnvironment(source_image, ENVIRONMENT_IRRADIANCE_WIDTH, ENVIRONMENT_IRRADIANCE_HEIGHT);
    m_environment_irradiance_texture_handle =
        resource_operator.CreateTexture(CreateTextureDescFromLinearImage(irradiance_image));

    std::vector<LinearEnvironmentImage> prefilter_mip_chain{};
    prefilter_mip_chain.reserve(ENVIRONMENT_PREFILTER_ROUGHNESS_LEVELS.size());
    for (std::size_t level_index = 0; level_index < ENVIRONMENT_PREFILTER_ROUGHNESS_LEVELS.size(); ++level_index)
    {
        prefilter_mip_chain.push_back(
            GeneratePrefilterEnvironment(
                source_image,
                ENVIRONMENT_PREFILTER_WIDTHS[level_index],
                ENVIRONMENT_PREFILTER_HEIGHTS[level_index],
                ENVIRONMENT_PREFILTER_ROUGHNESS_LEVELS[level_index]));
    }
    m_environment_prefilter_texture_handle =
        resource_operator.CreateTexture(CreateTextureDescFromLinearImageMipChain(prefilter_mip_chain));

    m_has_texture_source = true;
}

void EnvironmentLightingResources::CreateFallbackTextureSet(RendererInterface::ResourceOperator& resource_operator)
{
    RendererInterface::TextureDesc fallback_texture_desc{};
    fallback_texture_desc.format = RendererInterface::RGBA8_UNORM;
    fallback_texture_desc.width = 1u;
    fallback_texture_desc.height = 1u;
    fallback_texture_desc.data.assign(
        FALLBACK_ENVIRONMENT_TEXTURE_RGBA.begin(),
        FALLBACK_ENVIRONMENT_TEXTURE_RGBA.end());

    m_environment_texture_handle = resource_operator.CreateTexture(fallback_texture_desc);
    m_environment_irradiance_texture_handle = resource_operator.CreateTexture(fallback_texture_desc);
    RendererInterface::TextureDesc fallback_prefilter_desc{};
    fallback_prefilter_desc.format = RendererInterface::RGBA8_UNORM;
    fallback_prefilter_desc.width = ENVIRONMENT_PREFILTER_WIDTHS[0];
    fallback_prefilter_desc.height = ENVIRONMENT_PREFILTER_HEIGHTS[0];
    fallback_prefilter_desc.generate_mipmaps = true;
    fallback_prefilter_desc.mip_levels.reserve(ENVIRONMENT_PREFILTER_ROUGHNESS_LEVELS.size());
    for (std::size_t level_index = 0; level_index < ENVIRONMENT_PREFILTER_ROUGHNESS_LEVELS.size(); ++level_index)
    {
        fallback_prefilter_desc.mip_levels.push_back(
            CreateSolidColorMipLevelDesc(
                ENVIRONMENT_PREFILTER_WIDTHS[level_index],
                ENVIRONMENT_PREFILTER_HEIGHTS[level_index],
                FALLBACK_ENVIRONMENT_TEXTURE_RGBA));
    }
    m_environment_prefilter_texture_handle = resource_operator.CreateTexture(fallback_prefilter_desc);

    m_has_texture_source = false;
}
