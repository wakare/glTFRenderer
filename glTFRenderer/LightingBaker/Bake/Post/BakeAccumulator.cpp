#include "BakeAccumulator.h"

#include "Bake/Atlas/LightmapAtlasBuilder.h"
#include "Bake/RayTracing/BakeRayTracingDispatch.h"
#include "SceneFileLoader/glTFImageIOUtil.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <system_error>
#include <vector>

#include <nlohmann_json/single_include/nlohmann/json.hpp>

namespace LightingBaker
{
    namespace
    {
        constexpr unsigned kExpectedTexelRecordStride = 56u;

        struct BakeAccumulatorAtlasCache
        {
            unsigned atlas_id{0};
            unsigned width{0};
            unsigned height{0};
            std::filesystem::path accumulation_path{};
            std::filesystem::path sample_count_path{};
            std::filesystem::path variance_path{};
            std::filesystem::path published_atlas_path{};
            std::vector<float> accumulation{};
            std::vector<std::uint32_t> sample_count{};
            std::vector<float> variance{};
        };

        std::filesystem::path NormalizePathForComparison(const std::filesystem::path& path)
        {
            if (path.empty())
            {
                return {};
            }

            std::error_code error_code{};
            const std::filesystem::path absolute_path =
                path.is_absolute() ? path : std::filesystem::absolute(path, error_code);
            if (!error_code)
            {
                return absolute_path.lexically_normal();
            }

            return path.lexically_normal();
        }

        bool ReadJsonFile(const std::filesystem::path& file_path, nlohmann::json& out_root, std::wstring& out_error)
        {
            std::ifstream stream(file_path, std::ios::in | std::ios::binary);
            if (!stream.is_open())
            {
                out_error = L"Failed to open resume metadata file: " + file_path.native();
                return false;
            }

            try
            {
                stream >> out_root;
            }
            catch (const std::exception& exception)
            {
                const std::string error_message = exception.what();
                out_error = L"Failed to parse resume metadata json: " + file_path.native() +
                            L" (" + std::wstring(error_message.begin(), error_message.end()) + L")";
                return false;
            }

            return true;
        }

        bool ReadUnsignedField(const nlohmann::json& root,
                               const char* field_name,
                               unsigned& out_value,
                               std::wstring& out_error)
        {
            const auto field_it = root.find(field_name);
            if (field_it == root.end() || !field_it->is_number_unsigned())
            {
                out_error = L"Resume metadata is missing an unsigned field: " +
                            std::wstring(field_name, field_name + std::strlen(field_name));
                return false;
            }

            out_value = field_it->get<unsigned>();
            return true;
        }

        bool ReadBoolField(const nlohmann::json& root,
                           const char* field_name,
                           bool& out_value,
                           std::wstring& out_error)
        {
            const auto field_it = root.find(field_name);
            if (field_it == root.end() || !field_it->is_boolean())
            {
                out_error = L"Resume metadata is missing a boolean field: " +
                            std::wstring(field_name, field_name + std::strlen(field_name));
                return false;
            }

            out_value = field_it->get<bool>();
            return true;
        }

        bool ReadPathField(const nlohmann::json& root,
                           const char* field_name,
                           std::filesystem::path& out_path,
                           std::wstring& out_error)
        {
            const auto field_it = root.find(field_name);
            if (field_it == root.end() || !field_it->is_string())
            {
                out_error = L"Resume metadata is missing a string path field: " +
                            std::wstring(field_name, field_name + std::strlen(field_name));
                return false;
            }

            out_path = std::filesystem::path(field_it->get<std::string>());
            return true;
        }

        unsigned CountAtlasTexelRecords(const LightmapAtlasBuildResult& atlas_result, unsigned atlas_id)
        {
            unsigned count = 0u;
            for (const LightmapAtlasTexelRecord& texel_record : atlas_result.texel_records)
            {
                if (texel_record.atlas_id == atlas_id)
                {
                    ++count;
                }
            }

            return count;
        }

        bool ValidateExistingFileSize(const std::filesystem::path& file_path,
                                      std::uint64_t expected_size,
                                      const wchar_t* label,
                                      std::wstring& out_error)
        {
            std::error_code error_code{};
            if (!std::filesystem::exists(file_path, error_code) || error_code)
            {
                out_error = std::wstring(L"Missing ") + label + L": " + file_path.native();
                return false;
            }

            const std::uint64_t actual_size = std::filesystem::file_size(file_path, error_code);
            if (error_code)
            {
                out_error = std::wstring(L"Failed to read size of ") + label + L": " + file_path.native();
                return false;
            }

            if (actual_size != expected_size)
            {
                out_error = std::wstring(L"Unexpected size for ") + label + L": " + file_path.native();
                return false;
            }

            return true;
        }

        template <typename TValue>
        bool ReadBinaryVector(const std::filesystem::path& file_path,
                              std::size_t element_count,
                              const wchar_t* label,
                              std::vector<TValue>& out_values,
                              std::wstring& out_error)
        {
            std::ifstream stream(file_path, std::ios::in | std::ios::binary);
            if (!stream.is_open())
            {
                out_error = std::wstring(L"Failed to open ") + label + L": " + file_path.native();
                return false;
            }

            out_values.resize(element_count);
            if (element_count == 0u)
            {
                return true;
            }

            const std::streamsize byte_count =
                static_cast<std::streamsize>(element_count * sizeof(TValue));
            stream.read(reinterpret_cast<char*>(out_values.data()), byte_count);
            if (!stream.good() && !stream.eof())
            {
                out_error = std::wstring(L"Failed while reading ") + label + L": " + file_path.native();
                return false;
            }

            if (stream.gcount() != byte_count)
            {
                out_error = std::wstring(L"Unexpected read size for ") + label + L": " + file_path.native();
                return false;
            }

            return true;
        }

        template <typename TValue>
        bool WriteBinaryVector(const std::filesystem::path& file_path,
                               const wchar_t* label,
                               const std::vector<TValue>& values,
                               std::wstring& out_error)
        {
            const std::filesystem::path parent_path = file_path.parent_path();
            if (!parent_path.empty())
            {
                std::error_code error_code{};
                std::filesystem::create_directories(parent_path, error_code);
                if (error_code)
                {
                    out_error = std::wstring(L"Failed to create output directory for ") + label + L": " +
                                parent_path.native();
                    return false;
                }
            }

            std::ofstream stream(file_path, std::ios::out | std::ios::trunc | std::ios::binary);
            if (!stream.is_open())
            {
                out_error = std::wstring(L"Failed to open ") + label + L" for write: " + file_path.native();
                return false;
            }

            if (!values.empty())
            {
                const std::streamsize byte_count =
                    static_cast<std::streamsize>(values.size() * sizeof(TValue));
                stream.write(reinterpret_cast<const char*>(values.data()), byte_count);
                if (!stream.good())
                {
                    out_error = std::wstring(L"Failed while writing ") + label + L": " + file_path.native();
                    return false;
                }
            }

            return true;
        }

        bool EnsureParentDirectory(const std::filesystem::path& file_path, std::wstring& out_error)
        {
            const std::filesystem::path parent_path = file_path.parent_path();
            if (parent_path.empty())
            {
                return true;
            }

            std::error_code error_code{};
            std::filesystem::create_directories(parent_path, error_code);
            if (error_code)
            {
                out_error = L"Failed to create output directory: " + parent_path.native();
                return false;
            }

            return true;
        }

        std::string PathToUtf8String(const std::filesystem::path& path)
        {
            const auto utf8_path = path.u8string();
            return std::string(reinterpret_cast<const char*>(utf8_path.data()), utf8_path.size());
        }

        std::uint64_t BuildBindingKey(unsigned stable_node_key, unsigned primitive_hash)
        {
            return (static_cast<std::uint64_t>(stable_node_key) << 32u) | static_cast<std::uint64_t>(primitive_hash);
        }

        std::filesystem::path BuildIndexedFileName(const wchar_t* prefix,
                                                   unsigned atlas_id,
                                                   const wchar_t* suffix)
        {
            std::wostringstream stream{};
            stream << prefix << std::setw(2) << std::setfill(L'0') << atlas_id << suffix;
            return std::filesystem::path(stream.str());
        }

        std::filesystem::path BuildMasterAtlasPath(const BakeAccumulatorResumeState& resume_state, unsigned atlas_id)
        {
            return resume_state.package_root / L"atlases" /
                   BuildIndexedFileName(L"indirect_", atlas_id, L".master.rgba16f.bin");
        }

        std::filesystem::path BuildPreviewAtlasPath(const BakeAccumulatorResumeState& resume_state, unsigned atlas_id)
        {
            return resume_state.package_root / L"atlases" /
                   BuildIndexedFileName(L"indirect_", atlas_id, L".preview.png");
        }

        std::filesystem::path BuildCoverageAtlasPath(const BakeAccumulatorResumeState& resume_state, unsigned atlas_id)
        {
            return resume_state.package_root / L"debug" / BuildIndexedFileName(L"coverage_", atlas_id, L".png");
        }

        std::filesystem::path BuildChartIdAtlasPath(const BakeAccumulatorResumeState& resume_state, unsigned atlas_id)
        {
            return resume_state.package_root / L"debug" / BuildIndexedFileName(L"chart_id_", atlas_id, L".png");
        }

        const BakeAccumulatorPublishedAtlasState* FindPublishedAtlas(const BakeAccumulatorResumeState& resume_state,
                                                                    unsigned atlas_id)
        {
            for (const BakeAccumulatorPublishedAtlasState& published_atlas : resume_state.published_atlases)
            {
                if (published_atlas.atlas_id == atlas_id)
                {
                    return &published_atlas;
                }
            }

            return nullptr;
        }

        glm::vec3 EvaluateDebugHemisphereRadiance(const LightmapAtlasTexelRecord& texel_record)
        {
            glm::vec3 normal = texel_record.geometric_normal;
            const float length_sqr = glm::dot(normal, normal);
            if (length_sqr > 1e-8f)
            {
                normal *= glm::inversesqrt(length_sqr);
            }
            else
            {
                normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }

            const float up_weight = glm::clamp(normal.y * 0.5f + 0.5f, 0.0f, 1.0f);
            const glm::vec3 sky_color(0.28f, 0.38f, 0.58f);
            const glm::vec3 horizon_color(0.16f, 0.17f, 0.20f);
            const glm::vec3 ground_color(0.08f, 0.06f, 0.05f);
            const glm::vec3 hemisphere_color =
                up_weight >= 0.5f
                    ? glm::mix(horizon_color, sky_color, (up_weight - 0.5f) * 2.0f)
                    : glm::mix(ground_color, horizon_color, up_weight * 2.0f);

            const float facing_term = (std::max)(0.2f, normal.y * 0.5f + 0.5f);
            return hemisphere_color * facing_term;
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

        std::uint16_t PackHalf(float value)
        {
            if (!std::isfinite(value))
            {
                value = 0.0f;
            }

            value = (std::max)(0.0f, (std::min)(value, 65504.0f));
            return FloatToHalfBits(value);
        }

        float SanitizeRadianceChannel(float value)
        {
            if (!std::isfinite(value))
            {
                return 0.0f;
            }

            return value;
        }

        glm::vec3 LoadDispatchRadiance(const BakeRayTracingDispatchRunResult& ray_tracing_dispatch,
                                      std::size_t pixel_index)
        {
            const std::size_t rgba_offset = pixel_index * 4u;
            if (rgba_offset + 2u >= ray_tracing_dispatch.output_rgba.size())
            {
                return glm::vec3(0.0f, 0.0f, 0.0f);
            }

            return glm::vec3(
                SanitizeRadianceChannel(ray_tracing_dispatch.output_rgba[rgba_offset + 0u]),
                SanitizeRadianceChannel(ray_tracing_dispatch.output_rgba[rgba_offset + 1u]),
                SanitizeRadianceChannel(ray_tracing_dispatch.output_rgba[rgba_offset + 2u]));
        }

        bool BuildAtlasHalfWords(unsigned width,
                                 unsigned height,
                                 const std::vector<float>& accumulation,
                                 const std::vector<std::uint32_t>& sample_count,
                                 std::vector<std::uint16_t>& out_half_words,
                                 std::wstring& out_error)
        {
            const std::size_t pixel_count =
                static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
            if (accumulation.size() != pixel_count * 3u || sample_count.size() != pixel_count)
            {
                out_error = L"Half-float atlas write received mismatched accumulation cache sizes.";
                return false;
            }

            std::vector<std::uint16_t> atlas_half_words(pixel_count * 4u, 0u);
            for (std::size_t pixel_index = 0u; pixel_index < pixel_count; ++pixel_index)
            {
                const std::uint32_t spp = sample_count[pixel_index];
                const float inv_spp = spp > 0u ? (1.0f / static_cast<float>(spp)) : 0.0f;
                const std::size_t accumulation_offset = pixel_index * 3u;
                const std::size_t atlas_offset = pixel_index * 4u;
                atlas_half_words[atlas_offset + 0u] = PackHalf(accumulation[accumulation_offset + 0u] * inv_spp);
                atlas_half_words[atlas_offset + 1u] = PackHalf(accumulation[accumulation_offset + 1u] * inv_spp);
                atlas_half_words[atlas_offset + 2u] = PackHalf(accumulation[accumulation_offset + 2u] * inv_spp);
                atlas_half_words[atlas_offset + 3u] = PackHalf(spp > 0u ? 1.0f : 0.0f);
            }

            out_half_words = std::move(atlas_half_words);
            return true;
        }

        bool WriteHalfFloatAtlas(const std::filesystem::path& file_path,
                                 const wchar_t* label,
                                 unsigned width,
                                 unsigned height,
                                 const std::vector<float>& accumulation,
                                 const std::vector<std::uint32_t>& sample_count,
                                 std::wstring& out_error)
        {
            std::vector<std::uint16_t> atlas_half_words{};
            if (!BuildAtlasHalfWords(width, height, accumulation, sample_count, atlas_half_words, out_error))
            {
                return false;
            }

            return WriteBinaryVector(file_path, label, atlas_half_words, out_error);
        }

        bool WritePublishedAtlas(const std::filesystem::path& file_path,
                                 unsigned width,
                                 unsigned height,
                                 const std::vector<float>& accumulation,
                                 const std::vector<std::uint32_t>& sample_count,
                                 std::wstring& out_error)
        {
            return WriteHalfFloatAtlas(file_path,
                                       L"published atlas",
                                       width,
                                       height,
                                       accumulation,
                                       sample_count,
                                       out_error);
        }

        std::uint8_t LinearToPreviewByte(float value)
        {
            if (!std::isfinite(value))
            {
                value = 0.0f;
            }

            value = (std::max)(0.0f, value);
            const float mapped = value / (1.0f + value);
            const float gamma_encoded = std::pow((std::max)(0.0f, (std::min)(mapped, 1.0f)), 1.0f / 2.2f);
            return static_cast<std::uint8_t>(gamma_encoded * 255.0f + 0.5f);
        }

        std::array<std::uint8_t, 4> BuildChartColor(unsigned chart_index)
        {
            std::uint32_t hash = chart_index + 1u;
            hash ^= hash >> 16u;
            hash *= 0x7feb352du;
            hash ^= hash >> 15u;
            hash *= 0x846ca68bu;
            hash ^= hash >> 16u;

            const auto channel_from_shift = [hash](unsigned shift)
            {
                return static_cast<std::uint8_t>(80u + ((hash >> shift) & 0xafu));
            };

            return {channel_from_shift(0u), channel_from_shift(8u), channel_from_shift(16u), 255u};
        }

        bool WritePngImage(const std::filesystem::path& file_path,
                           const wchar_t* label,
                           const std::vector<std::uint8_t>& rgba_data,
                           unsigned width,
                           unsigned height,
                           std::wstring& out_error)
        {
            const std::size_t pixel_count =
                static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
            if (rgba_data.size() != pixel_count * 4u)
            {
                out_error = std::wstring(L"PNG image write received mismatched pixel data for ") + label + L".";
                return false;
            }

            if (!EnsureParentDirectory(file_path, out_error))
            {
                return false;
            }

            if (!glTFImageIOUtil::Instance().SaveAsPNG(PathToUtf8String(file_path),
                                                       rgba_data.data(),
                                                       static_cast<int>(width),
                                                       static_cast<int>(height)))
            {
                out_error = std::wstring(L"Failed to write ") + label + L": " + file_path.native();
                return false;
            }

            return true;
        }

        bool WritePreviewAtlasPNG(const std::filesystem::path& file_path,
                                  unsigned width,
                                  unsigned height,
                                  const std::vector<float>& accumulation,
                                  const std::vector<std::uint32_t>& sample_count,
                                  std::wstring& out_error)
        {
            const std::size_t pixel_count =
                static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
            if (accumulation.size() != pixel_count * 3u || sample_count.size() != pixel_count)
            {
                out_error = L"Preview atlas write received mismatched accumulation cache sizes.";
                return false;
            }

            std::vector<std::uint8_t> rgba_data(pixel_count * 4u, 0u);
            for (std::size_t pixel_index = 0u; pixel_index < pixel_count; ++pixel_index)
            {
                const std::uint32_t spp = sample_count[pixel_index];
                const float inv_spp = spp > 0u ? (1.0f / static_cast<float>(spp)) : 0.0f;
                const std::size_t accumulation_offset = pixel_index * 3u;
                const std::size_t rgba_offset = pixel_index * 4u;
                rgba_data[rgba_offset + 0u] =
                    LinearToPreviewByte(accumulation[accumulation_offset + 0u] * inv_spp);
                rgba_data[rgba_offset + 1u] =
                    LinearToPreviewByte(accumulation[accumulation_offset + 1u] * inv_spp);
                rgba_data[rgba_offset + 2u] =
                    LinearToPreviewByte(accumulation[accumulation_offset + 2u] * inv_spp);
                rgba_data[rgba_offset + 3u] = 255u;
            }

            return WritePngImage(file_path, L"preview atlas png", rgba_data, width, height, out_error);
        }

        bool WriteCoverageAtlasPNG(const std::filesystem::path& file_path,
                                   unsigned atlas_id,
                                   unsigned width,
                                   unsigned height,
                                   const LightmapAtlasBuildResult& atlas_result,
                                   std::wstring& out_error)
        {
            const std::size_t pixel_count =
                static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
            std::vector<std::uint8_t> rgba_data(pixel_count * 4u, 0u);
            for (std::size_t pixel_index = 0u; pixel_index < pixel_count; ++pixel_index)
            {
                rgba_data[pixel_index * 4u + 3u] = 255u;
            }

            for (const LightmapAtlasTexelRecord& texel_record : atlas_result.texel_records)
            {
                if (texel_record.atlas_id != atlas_id)
                {
                    continue;
                }

                if (texel_record.texel_x >= width || texel_record.texel_y >= height)
                {
                    out_error = L"Coverage atlas write encountered an out-of-range texel record.";
                    return false;
                }

                const std::size_t pixel_index =
                    static_cast<std::size_t>(texel_record.texel_y) * static_cast<std::size_t>(width) +
                    static_cast<std::size_t>(texel_record.texel_x);
                const std::size_t rgba_offset = pixel_index * 4u;
                rgba_data[rgba_offset + 0u] = 255u;
                rgba_data[rgba_offset + 1u] = 255u;
                rgba_data[rgba_offset + 2u] = 255u;
            }

            return WritePngImage(file_path, L"coverage atlas png", rgba_data, width, height, out_error);
        }

        bool WriteChartIdAtlasPNG(const std::filesystem::path& file_path,
                                  unsigned atlas_id,
                                  unsigned width,
                                  unsigned height,
                                  const LightmapAtlasBuildResult& atlas_result,
                                  std::wstring& out_error)
        {
            const std::size_t pixel_count =
                static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
            const unsigned unassigned_chart = std::numeric_limits<unsigned>::max();
            const unsigned overlap_chart = std::numeric_limits<unsigned>::max() - 1u;

            std::map<std::uint64_t, unsigned> chart_indices{};
            unsigned next_chart_index = 0u;
            for (const LightmapAtlasBindingInfo& binding : atlas_result.bindings)
            {
                if (binding.atlas_id != atlas_id)
                {
                    continue;
                }

                const std::uint64_t binding_key = BuildBindingKey(binding.stable_node_key, binding.primitive_hash);
                const auto insert_result = chart_indices.emplace(binding_key, next_chart_index);
                if (insert_result.second)
                {
                    ++next_chart_index;
                }
            }

            std::vector<unsigned> pixel_charts(pixel_count, unassigned_chart);
            for (const LightmapAtlasTexelRecord& texel_record : atlas_result.texel_records)
            {
                if (texel_record.atlas_id != atlas_id)
                {
                    continue;
                }

                if (texel_record.texel_x >= width || texel_record.texel_y >= height)
                {
                    out_error = L"Chart-id atlas write encountered an out-of-range texel record.";
                    return false;
                }

                const std::uint64_t binding_key =
                    BuildBindingKey(texel_record.stable_node_key, texel_record.primitive_hash);
                auto chart_it = chart_indices.find(binding_key);
                if (chart_it == chart_indices.end())
                {
                    const unsigned chart_index = next_chart_index++;
                    chart_it = chart_indices.emplace(binding_key, chart_index).first;
                }

                const std::size_t pixel_index =
                    static_cast<std::size_t>(texel_record.texel_y) * static_cast<std::size_t>(width) +
                    static_cast<std::size_t>(texel_record.texel_x);
                if (pixel_charts[pixel_index] == unassigned_chart)
                {
                    pixel_charts[pixel_index] = chart_it->second;
                }
                else if (pixel_charts[pixel_index] != chart_it->second)
                {
                    pixel_charts[pixel_index] = overlap_chart;
                }
            }

            std::vector<std::uint8_t> rgba_data(pixel_count * 4u, 0u);
            for (std::size_t pixel_index = 0u; pixel_index < pixel_count; ++pixel_index)
            {
                const std::size_t rgba_offset = pixel_index * 4u;
                rgba_data[rgba_offset + 3u] = 255u;

                const unsigned chart_index = pixel_charts[pixel_index];
                if (chart_index == unassigned_chart)
                {
                    continue;
                }

                const std::array<std::uint8_t, 4> color =
                    chart_index == overlap_chart ? std::array<std::uint8_t, 4>{255u, 0u, 255u, 255u}
                                                 : BuildChartColor(chart_index);
                rgba_data[rgba_offset + 0u] = color[0];
                rgba_data[rgba_offset + 1u] = color[1];
                rgba_data[rgba_offset + 2u] = color[2];
                rgba_data[rgba_offset + 3u] = color[3];
            }

            return WritePngImage(file_path, L"chart-id atlas png", rgba_data, width, height, out_error);
        }

        bool WriteAtlasArtifacts(const BakeAccumulatorResumeState& resume_state,
                                 const LightmapAtlasBuildResult& atlas_result,
                                 const BakeAccumulatorAtlasCache& atlas_cache,
                                 std::wstring& out_error)
        {
            if (!WriteHalfFloatAtlas(BuildMasterAtlasPath(resume_state, atlas_cache.atlas_id),
                                     L"master atlas",
                                     atlas_cache.width,
                                     atlas_cache.height,
                                     atlas_cache.accumulation,
                                     atlas_cache.sample_count,
                                     out_error))
            {
                return false;
            }

            if (!WritePreviewAtlasPNG(BuildPreviewAtlasPath(resume_state, atlas_cache.atlas_id),
                                      atlas_cache.width,
                                      atlas_cache.height,
                                      atlas_cache.accumulation,
                                      atlas_cache.sample_count,
                                      out_error))
            {
                return false;
            }

            if (!WriteCoverageAtlasPNG(BuildCoverageAtlasPath(resume_state, atlas_cache.atlas_id),
                                       atlas_cache.atlas_id,
                                       atlas_cache.width,
                                       atlas_cache.height,
                                       atlas_result,
                                       out_error))
            {
                return false;
            }

            if (!WriteChartIdAtlasPNG(BuildChartIdAtlasPath(resume_state, atlas_cache.atlas_id),
                                      atlas_cache.atlas_id,
                                      atlas_cache.width,
                                      atlas_cache.height,
                                      atlas_result,
                                      out_error))
            {
                return false;
            }

            return true;
        }

        bool LoadAtlasCache(const BakeAccumulatorResumeState& resume_state,
                            const BakeAccumulatorAtlasInputState& atlas_input,
                            const BakeAccumulatorPublishedAtlasState& published_atlas,
                            BakeAccumulatorAtlasCache& out_cache,
                            std::wstring& out_error)
        {
            const std::size_t pixel_count =
                static_cast<std::size_t>(published_atlas.width) * static_cast<std::size_t>(published_atlas.height);
            if (pixel_count == 0u)
            {
                out_error = L"Published atlas dimensions must be non-zero.";
                return false;
            }

            BakeAccumulatorAtlasCache cache{};
            cache.atlas_id = atlas_input.atlas_id;
            cache.width = published_atlas.width;
            cache.height = published_atlas.height;
            cache.accumulation_path = resume_state.cache_root / atlas_input.accumulation_file;
            cache.sample_count_path = resume_state.cache_root / atlas_input.sample_count_file;
            cache.variance_path = resume_state.cache_root / atlas_input.variance_file;
            cache.published_atlas_path = resume_state.package_root / published_atlas.atlas_file;

            if (!ReadBinaryVector(cache.accumulation_path,
                                  pixel_count * 3u,
                                  L"accumulation cache file",
                                  cache.accumulation,
                                  out_error) ||
                !ReadBinaryVector(cache.sample_count_path,
                                  pixel_count,
                                  L"sample-count cache file",
                                  cache.sample_count,
                                  out_error) ||
                !ReadBinaryVector(cache.variance_path,
                                  pixel_count,
                                  L"variance cache file",
                                  cache.variance,
                                  out_error))
            {
                return false;
            }

            out_cache = std::move(cache);
            return true;
        }
    }

    bool BakeAccumulator::LoadResumeState(const std::filesystem::path& resume_path,
                                          BakeAccumulatorResumeState& out_state,
                                          std::wstring& out_error) const
    {
        nlohmann::json root{};
        if (!ReadJsonFile(resume_path, root, out_error))
        {
            return false;
        }

        BakeAccumulatorResumeState state{};
        state.resume_path = resume_path;
        state.cache_root = resume_path.parent_path();
        state.package_root = state.cache_root.parent_path();
        if (!ReadPathField(root, "source_scene", state.source_scene, out_error))
        {
            return false;
        }

        const auto job_it = root.find("job");
        if (job_it == root.end() || !job_it->is_object())
        {
            out_error = L"Resume metadata is missing the 'job' object.";
            return false;
        }

        if (!ReadUnsignedField(*job_it, "atlas_resolution", state.atlas_resolution, out_error) ||
            !ReadUnsignedField(*job_it, "samples_per_iteration", state.samples_per_iteration, out_error) ||
            !ReadUnsignedField(*job_it, "target_samples", state.target_samples, out_error) ||
            !ReadUnsignedField(*job_it, "max_bounces", state.max_bounces, out_error) ||
            !ReadBoolField(*job_it, "progressive", state.progressive, out_error))
        {
            return false;
        }

        const auto progress_it = root.find("progress");
        if (progress_it == root.end() || !progress_it->is_object())
        {
            out_error = L"Resume metadata is missing the 'progress' object.";
            return false;
        }

        if (!ReadUnsignedField(*progress_it, "completed_samples", state.completed_samples, out_error) ||
            !ReadBoolField(*progress_it, "has_accumulation_cache", state.has_accumulation_cache, out_error))
        {
            return false;
        }

        const auto published_atlases_it = root.find("published_atlases");
        if (published_atlases_it == root.end() || !published_atlases_it->is_array())
        {
            out_error = L"Resume metadata is missing the 'published_atlases' array.";
            return false;
        }

        for (const auto& published_atlas_json : *published_atlases_it)
        {
            if (!published_atlas_json.is_object())
            {
                out_error = L"Resume metadata contains a malformed published atlas entry.";
                return false;
            }

            BakeAccumulatorPublishedAtlasState published_atlas{};
            if (!ReadUnsignedField(published_atlas_json, "atlas_id", published_atlas.atlas_id, out_error) ||
                !ReadUnsignedField(published_atlas_json, "width", published_atlas.width, out_error) ||
                !ReadUnsignedField(published_atlas_json, "height", published_atlas.height, out_error) ||
                !ReadPathField(published_atlas_json, "file", published_atlas.atlas_file, out_error))
            {
                return false;
            }

            state.published_atlases.push_back(std::move(published_atlas));
        }

        const auto atlas_inputs_it = root.find("atlas_inputs");
        if (atlas_inputs_it == root.end() || !atlas_inputs_it->is_array())
        {
            out_error = L"Resume metadata is missing the 'atlas_inputs' array.";
            return false;
        }

        for (const auto& atlas_input_json : *atlas_inputs_it)
        {
            if (!atlas_input_json.is_object())
            {
                out_error = L"Resume metadata contains a malformed atlas input entry.";
                return false;
            }

            BakeAccumulatorAtlasInputState atlas_input{};
            if (!ReadUnsignedField(atlas_input_json, "atlas_id", atlas_input.atlas_id, out_error) ||
                !ReadUnsignedField(atlas_input_json, "texel_record_count", atlas_input.texel_record_count, out_error) ||
                !ReadUnsignedField(atlas_input_json, "texel_record_stride", atlas_input.texel_record_stride, out_error) ||
                !ReadPathField(atlas_input_json, "texel_record_file", atlas_input.texel_record_file, out_error) ||
                !ReadPathField(atlas_input_json, "accumulation_file", atlas_input.accumulation_file, out_error) ||
                !ReadPathField(atlas_input_json, "sample_count_file", atlas_input.sample_count_file, out_error) ||
                !ReadPathField(atlas_input_json, "variance_file", atlas_input.variance_file, out_error))
            {
                return false;
            }

            state.atlas_inputs.push_back(std::move(atlas_input));
        }

        out_state = std::move(state);
        return true;
    }

    bool BakeAccumulator::ValidateResumeState(const BakeAccumulatorResumeState& resume_state,
                                              const BakeJobConfig& config,
                                              const LightmapAtlasBuildResult& atlas_result,
                                              std::wstring& out_error) const
    {
        if (NormalizePathForComparison(resume_state.source_scene) != NormalizePathForComparison(config.scene_path))
        {
            out_error = L"Resume cache scene path does not match the requested bake scene.";
            return false;
        }

        if (resume_state.atlas_resolution != config.atlas_resolution)
        {
            out_error = L"Resume cache atlas resolution does not match the requested job.";
            return false;
        }

        if (resume_state.samples_per_iteration != config.samples_per_iteration)
        {
            out_error = L"Resume cache samples-per-iteration does not match the requested job.";
            return false;
        }

        if (resume_state.target_samples != config.target_samples)
        {
            out_error = L"Resume cache target sample count does not match the requested job.";
            return false;
        }

        if (resume_state.max_bounces != config.max_bounces)
        {
            out_error = L"Resume cache max-bounces does not match the requested job.";
            return false;
        }

        if (resume_state.progressive != config.progressive)
        {
            out_error = L"Resume cache progressive mode does not match the requested job.";
            return false;
        }

        if (resume_state.completed_samples > resume_state.target_samples)
        {
            out_error = L"Resume cache has an invalid completed sample count.";
            return false;
        }

        if (resume_state.atlas_inputs.empty())
        {
            out_error = L"Resume cache does not contain any atlas input entries.";
            return false;
        }

        if (resume_state.published_atlases.empty())
        {
            out_error = L"Resume cache does not contain any published atlas entries.";
            return false;
        }

        for (const BakeAccumulatorPublishedAtlasState& published_atlas : resume_state.published_atlases)
        {
            if (published_atlas.width != resume_state.atlas_resolution ||
                published_atlas.height != resume_state.atlas_resolution)
            {
                out_error = L"Resume cache published atlas dimensions do not match the bake atlas resolution.";
                return false;
            }
        }

        const std::uint64_t pixel_count =
            static_cast<std::uint64_t>(resume_state.atlas_resolution) *
            static_cast<std::uint64_t>(resume_state.atlas_resolution);

        for (const BakeAccumulatorAtlasInputState& atlas_input : resume_state.atlas_inputs)
        {
            const unsigned expected_texel_record_count = CountAtlasTexelRecords(atlas_result, atlas_input.atlas_id);
            if (atlas_input.texel_record_count != expected_texel_record_count)
            {
                out_error = L"Resume cache texel record count does not match the current atlas build.";
                return false;
            }

            if (atlas_input.texel_record_stride != kExpectedTexelRecordStride)
            {
                out_error = L"Resume cache texel record stride does not match the supported disk layout.";
                return false;
            }

            const BakeAccumulatorPublishedAtlasState* published_atlas =
                FindPublishedAtlas(resume_state, atlas_input.atlas_id);
            if (published_atlas == nullptr)
            {
                out_error = L"Resume cache is missing the published atlas entry for an atlas input.";
                return false;
            }

            const std::filesystem::path texel_record_path = resume_state.cache_root / atlas_input.texel_record_file;
            if (!ValidateExistingFileSize(texel_record_path,
                                          static_cast<std::uint64_t>(atlas_input.texel_record_count) * atlas_input.texel_record_stride,
                                          L"texel record cache file",
                                          out_error))
            {
                return false;
            }

            const std::filesystem::path accumulation_path = resume_state.cache_root / atlas_input.accumulation_file;
            if (!ValidateExistingFileSize(accumulation_path,
                                          pixel_count * 3ull * sizeof(float),
                                          L"accumulation cache file",
                                          out_error))
            {
                return false;
            }

            const std::filesystem::path sample_count_path = resume_state.cache_root / atlas_input.sample_count_file;
            if (!ValidateExistingFileSize(sample_count_path,
                                          pixel_count * sizeof(std::uint32_t),
                                          L"sample-count cache file",
                                          out_error))
            {
                return false;
            }

            const std::filesystem::path variance_path = resume_state.cache_root / atlas_input.variance_file;
            if (!ValidateExistingFileSize(variance_path,
                                          pixel_count * sizeof(float),
                                          L"variance cache file",
                                          out_error))
            {
                return false;
            }

            const std::filesystem::path published_atlas_path = resume_state.package_root / published_atlas->atlas_file;
            if (!ValidateExistingFileSize(published_atlas_path,
                                          pixel_count * 4ull * sizeof(std::uint16_t),
                                          L"published atlas file",
                                          out_error))
            {
                return false;
            }
        }

        return true;
    }

    bool BakeAccumulator::AccumulateRayTracingDispatchOutput(const BakeAccumulatorResumeState& resume_state,
                                                             const BakeJobConfig& config,
                                                             const LightmapAtlasBuildResult& atlas_result,
                                                             const BakeRayTracingDispatchRunResult& ray_tracing_dispatch,
                                                             BakeAccumulationRunResult& out_result,
                                                             std::wstring& out_error) const
    {
        BakeAccumulationRunResult result{};
        result.previous_completed_samples = resume_state.completed_samples;
        result.completed_samples = resume_state.completed_samples;
        result.target_samples = config.target_samples;

        if (resume_state.atlas_inputs.empty())
        {
            out_error = L"Accumulation requires at least one atlas input.";
            return false;
        }

        if (!ray_tracing_dispatch.output_payload_valid)
        {
            out_error = L"Accumulation requires a valid ray tracing dispatch output payload.";
            return false;
        }

        const unsigned remaining_samples =
            resume_state.completed_samples < config.target_samples
                ? (config.target_samples - resume_state.completed_samples)
                : 0u;
        const unsigned samples_to_add =
            config.progressive ? (std::min)(config.samples_per_iteration, remaining_samples) : remaining_samples;

        const std::size_t expected_output_value_count =
            static_cast<std::size_t>(ray_tracing_dispatch.output_width) *
            static_cast<std::size_t>(ray_tracing_dispatch.output_height) * 4u;
        if (ray_tracing_dispatch.output_rgba.size() != expected_output_value_count)
        {
            out_error = L"Ray tracing dispatch output payload size does not match dispatch dimensions.";
            return false;
        }

        for (const BakeAccumulatorAtlasInputState& atlas_input : resume_state.atlas_inputs)
        {
            const BakeAccumulatorPublishedAtlasState* published_atlas =
                FindPublishedAtlas(resume_state, atlas_input.atlas_id);
            if (published_atlas == nullptr)
            {
                out_error = L"Accumulation could not resolve a published atlas path.";
                return false;
            }

            BakeAccumulatorAtlasCache atlas_cache{};
            if (!LoadAtlasCache(resume_state, atlas_input, *published_atlas, atlas_cache, out_error))
            {
                return false;
            }

            if (atlas_cache.width != ray_tracing_dispatch.output_width ||
                atlas_cache.height != ray_tracing_dispatch.output_height)
            {
                out_error = L"Ray tracing dispatch output dimensions do not match the atlas cache dimensions.";
                return false;
            }

            if (samples_to_add > 0u)
            {
                for (const LightmapAtlasTexelRecord& texel_record : atlas_result.texel_records)
                {
                    if (texel_record.atlas_id != atlas_input.atlas_id)
                    {
                        continue;
                    }

                    const std::size_t pixel_index =
                        static_cast<std::size_t>(texel_record.texel_y) * static_cast<std::size_t>(atlas_cache.width) +
                        static_cast<std::size_t>(texel_record.texel_x);
                    if (pixel_index >= atlas_cache.sample_count.size())
                    {
                        out_error = L"Atlas texel record referenced a texel outside the cache image.";
                        return false;
                    }

                    const glm::vec3 radiance = LoadDispatchRadiance(ray_tracing_dispatch, pixel_index);
                    const std::size_t accumulation_offset = pixel_index * 3u;
                    atlas_cache.accumulation[accumulation_offset + 0u] += radiance.x * static_cast<float>(samples_to_add);
                    atlas_cache.accumulation[accumulation_offset + 1u] += radiance.y * static_cast<float>(samples_to_add);
                    atlas_cache.accumulation[accumulation_offset + 2u] += radiance.z * static_cast<float>(samples_to_add);

                    const std::uint64_t new_sample_count =
                        static_cast<std::uint64_t>(atlas_cache.sample_count[pixel_index]) +
                        static_cast<std::uint64_t>(samples_to_add);
                    const std::uint64_t clamped_sample_count =
                        (std::min)(new_sample_count,
                                   static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()));
                    atlas_cache.sample_count[pixel_index] =
                        static_cast<std::uint32_t>(clamped_sample_count);
                    atlas_cache.variance[pixel_index] = 0.0f;
                }

                if (!WriteBinaryVector(atlas_cache.accumulation_path,
                                       L"accumulation cache file",
                                       atlas_cache.accumulation,
                                       out_error) ||
                    !WriteBinaryVector(atlas_cache.sample_count_path,
                                       L"sample-count cache file",
                                       atlas_cache.sample_count,
                                       out_error) ||
                    !WriteBinaryVector(atlas_cache.variance_path,
                                       L"variance cache file",
                                       atlas_cache.variance,
                                       out_error))
                {
                    return false;
                }

                result.cache_files_updated = true;
            }

            if (!WritePublishedAtlas(atlas_cache.published_atlas_path,
                                     atlas_cache.width,
                                     atlas_cache.height,
                                     atlas_cache.accumulation,
                                     atlas_cache.sample_count,
                                     out_error))
            {
                return false;
            }

            if (!WriteAtlasArtifacts(resume_state, atlas_result, atlas_cache, out_error))
            {
                return false;
            }

            result.published_atlases_updated = true;
        }

        result.added_samples = samples_to_add;
        result.completed_samples = resume_state.completed_samples + samples_to_add;
        out_result = result;
        return true;
    }

    bool BakeAccumulator::AccumulateDebugHemispherePreview(const BakeAccumulatorResumeState& resume_state,
                                                           const BakeJobConfig& config,
                                                           const LightmapAtlasBuildResult& atlas_result,
                                                           BakeAccumulationRunResult& out_result,
                                                           std::wstring& out_error) const
    {
        BakeAccumulationRunResult result{};
        result.previous_completed_samples = resume_state.completed_samples;
        result.completed_samples = resume_state.completed_samples;
        result.target_samples = config.target_samples;

        if (resume_state.atlas_inputs.empty())
        {
            out_error = L"Accumulation preview requires at least one atlas input.";
            return false;
        }

        const unsigned remaining_samples =
            resume_state.completed_samples < config.target_samples
                ? (config.target_samples - resume_state.completed_samples)
                : 0u;
        const unsigned samples_to_add =
            config.progressive ? (std::min)(config.samples_per_iteration, remaining_samples) : remaining_samples;

        for (const BakeAccumulatorAtlasInputState& atlas_input : resume_state.atlas_inputs)
        {
            const BakeAccumulatorPublishedAtlasState* published_atlas =
                FindPublishedAtlas(resume_state, atlas_input.atlas_id);
            if (published_atlas == nullptr)
            {
                out_error = L"Accumulation preview could not resolve a published atlas path.";
                return false;
            }

            BakeAccumulatorAtlasCache atlas_cache{};
            if (!LoadAtlasCache(resume_state, atlas_input, *published_atlas, atlas_cache, out_error))
            {
                return false;
            }

            if (samples_to_add > 0u)
            {
                for (const LightmapAtlasTexelRecord& texel_record : atlas_result.texel_records)
                {
                    if (texel_record.atlas_id != atlas_input.atlas_id)
                    {
                        continue;
                    }

                    const std::size_t pixel_index =
                        static_cast<std::size_t>(texel_record.texel_y) * static_cast<std::size_t>(atlas_cache.width) +
                        static_cast<std::size_t>(texel_record.texel_x);
                    if (pixel_index >= atlas_cache.sample_count.size())
                    {
                        out_error = L"Atlas texel record referenced a texel outside the cache image.";
                        return false;
                    }

                    const glm::vec3 radiance = EvaluateDebugHemisphereRadiance(texel_record);
                    const std::size_t accumulation_offset = pixel_index * 3u;
                    atlas_cache.accumulation[accumulation_offset + 0u] += radiance.x * static_cast<float>(samples_to_add);
                    atlas_cache.accumulation[accumulation_offset + 1u] += radiance.y * static_cast<float>(samples_to_add);
                    atlas_cache.accumulation[accumulation_offset + 2u] += radiance.z * static_cast<float>(samples_to_add);

                    const std::uint64_t new_sample_count =
                        static_cast<std::uint64_t>(atlas_cache.sample_count[pixel_index]) +
                        static_cast<std::uint64_t>(samples_to_add);
                    const std::uint64_t clamped_sample_count =
                        (std::min)(new_sample_count,
                                   static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()));
                    atlas_cache.sample_count[pixel_index] =
                        static_cast<std::uint32_t>(clamped_sample_count);
                    atlas_cache.variance[pixel_index] = 0.0f;
                }

                if (!WriteBinaryVector(atlas_cache.accumulation_path,
                                       L"accumulation cache file",
                                       atlas_cache.accumulation,
                                       out_error) ||
                    !WriteBinaryVector(atlas_cache.sample_count_path,
                                       L"sample-count cache file",
                                       atlas_cache.sample_count,
                                       out_error) ||
                    !WriteBinaryVector(atlas_cache.variance_path,
                                       L"variance cache file",
                                       atlas_cache.variance,
                                       out_error))
                {
                    return false;
                }

                result.cache_files_updated = true;
            }

            if (!WritePublishedAtlas(atlas_cache.published_atlas_path,
                                     atlas_cache.width,
                                     atlas_cache.height,
                                     atlas_cache.accumulation,
                                     atlas_cache.sample_count,
                                     out_error))
            {
                return false;
            }

            if (!WriteAtlasArtifacts(resume_state, atlas_result, atlas_cache, out_error))
            {
                return false;
            }

            result.published_atlases_updated = true;
        }

        result.added_samples = samples_to_add;
        result.completed_samples = resume_state.completed_samples + samples_to_add;
        out_result = result;
        return true;
    }
}
