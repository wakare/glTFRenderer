#include "BakeOutputWriter.h"

#include "Bake/Atlas/LightmapAtlasBuilder.h"
#include "Scene/BakeSceneImporter.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

#include <nlohmann_json/single_include/nlohmann/json.hpp>

namespace LightingBaker
{
    namespace
    {
        constexpr unsigned kManifestSchemaVersion = 1;
        constexpr const char* kGeneratorName = "LightingBaker";
        constexpr const char* kGeneratorPhase = "phase_a_scaffold";
        constexpr const char* kDefaultProfile = "default";
        constexpr const char* kDefaultAtlasRelativePath = "atlases/indirect_00.rgba16f.bin";
        constexpr const char* kResumeMetadataRelativePath = "cache/resume.json";
        constexpr const char* kImportSummaryRelativePath = "debug/import_summary.json";
        constexpr const char* kAtlasSummaryRelativePath = "debug/atlas_summary.json";

        struct BakeTexelRecordDiskV1
        {
            std::uint32_t atlas_id{0u};
            std::uint32_t texel_x{0u};
            std::uint32_t texel_y{0u};
            std::uint32_t stable_node_key{0xffffffffu};
            std::uint32_t primitive_hash{0xffffffffu};
            std::uint32_t triangle_index{0u};
            float atlas_u{0.0f};
            float atlas_v{0.0f};
            float world_x{0.0f};
            float world_y{0.0f};
            float world_z{0.0f};
            float geometric_normal_x{0.0f};
            float geometric_normal_y{1.0f};
            float geometric_normal_z{0.0f};
        };

        static_assert(sizeof(BakeTexelRecordDiskV1) == 56u, "Unexpected texel record disk stride.");

        std::string ToJsonPathString(const std::filesystem::path& path)
        {
            return path.generic_string();
        }

        std::string BuildUtcTimestampString()
        {
            const std::time_t now = std::time(nullptr);
            std::tm utc_tm{};
            gmtime_s(&utc_tm, &now);

            std::ostringstream stream{};
            stream << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%SZ");
            return stream.str();
        }

        nlohmann::json ToJson(const std::array<float, 4>& value)
        {
            return nlohmann::json::array({value[0], value[1], value[2], value[3]});
        }

        nlohmann::json ToJson(const glm::fvec2& value)
        {
            return nlohmann::json::array({value.x, value.y});
        }

        nlohmann::json ToJson(const BakeSceneValidationMessage& value)
        {
            return nlohmann::json{
                {"code", value.code},
                {"message", value.message},
            };
        }

        unsigned CountErrors(const BakeSceneImportResult& import_result)
        {
            unsigned count = static_cast<unsigned>(import_result.errors.size());
            for (const BakePrimitiveImportInfo& primitive : import_result.primitive_instances)
            {
                count += static_cast<unsigned>(primitive.errors.size());
            }

            return count;
        }

        unsigned CountWarnings(const BakeSceneImportResult& import_result)
        {
            unsigned count = static_cast<unsigned>(import_result.warnings.size());
            for (const BakePrimitiveImportInfo& primitive : import_result.primitive_instances)
            {
                count += static_cast<unsigned>(primitive.warnings.size());
            }

            return count;
        }

        unsigned CountErrors(const LightmapAtlasBuildResult& atlas_result)
        {
            return static_cast<unsigned>(atlas_result.errors.size());
        }

        unsigned CountWarnings(const LightmapAtlasBuildResult& atlas_result)
        {
            return static_cast<unsigned>(atlas_result.warnings.size());
        }

        unsigned CountPackageErrors(const BakeSceneImportResult& import_result, const LightmapAtlasBuildResult* atlas_result)
        {
            unsigned count = CountErrors(import_result);
            if (atlas_result != nullptr)
            {
                count += CountErrors(*atlas_result);
            }

            return count;
        }

        unsigned CountPackageWarnings(const BakeSceneImportResult& import_result, const LightmapAtlasBuildResult* atlas_result)
        {
            unsigned count = CountWarnings(import_result);
            if (atlas_result != nullptr)
            {
                count += CountWarnings(*atlas_result);
            }

            return count;
        }

        std::string ResolveSceneValidationStatus(const BakeSceneImportResult& import_result)
        {
            if (!import_result.load_succeeded)
            {
                return "load_failed";
            }

            return import_result.HasValidationErrors() ? "failed" : "passed";
        }

        std::string ResolveAtlasValidationStatus(const LightmapAtlasBuildResult* atlas_result)
        {
            if (atlas_result == nullptr)
            {
                return "not_built";
            }

            return atlas_result->HasValidationErrors() ? "failed" : "passed";
        }

        std::string ResolvePackageValidationStatus(const BakeSceneImportResult& import_result,
                                                   const LightmapAtlasBuildResult* atlas_result)
        {
            if (!import_result.load_succeeded)
            {
                return "load_failed";
            }

            if (import_result.HasValidationErrors())
            {
                return "failed";
            }

            if (atlas_result == nullptr)
            {
                return "pending_atlas";
            }

            return atlas_result->HasValidationErrors() ? "failed" : "passed";
        }

        std::string BuildCacheRelativePath(const char* stem, unsigned atlas_id, const char* suffix)
        {
            std::ostringstream stream{};
            stream << "cache/" << stem << "_" << std::setw(2) << std::setfill('0') << atlas_id << suffix;
            return stream.str();
        }

        BakeTexelRecordDiskV1 ToDiskRecord(const LightmapAtlasTexelRecord& texel_record)
        {
            BakeTexelRecordDiskV1 disk_record{};
            disk_record.atlas_id = texel_record.atlas_id;
            disk_record.texel_x = texel_record.texel_x;
            disk_record.texel_y = texel_record.texel_y;
            disk_record.stable_node_key = texel_record.stable_node_key;
            disk_record.primitive_hash = texel_record.primitive_hash;
            disk_record.triangle_index = texel_record.triangle_index;
            disk_record.atlas_u = texel_record.atlas_uv.x;
            disk_record.atlas_v = texel_record.atlas_uv.y;
            disk_record.world_x = texel_record.world_position.x;
            disk_record.world_y = texel_record.world_position.y;
            disk_record.world_z = texel_record.world_position.z;
            disk_record.geometric_normal_x = texel_record.geometric_normal.x;
            disk_record.geometric_normal_y = texel_record.geometric_normal.y;
            disk_record.geometric_normal_z = texel_record.geometric_normal.z;
            return disk_record;
        }

        void BuildPackageDescriptors(const BakeJobConfig& config,
                                     const BakeSceneImportResult& import_result,
                                     const LightmapAtlasBuildResult* atlas_result,
                                     std::vector<BakePublishedAtlasDesc>& out_atlas_descs,
                                     std::vector<BakePublishedBindingDesc>& out_binding_descs,
                                     std::vector<BakeAtlasCacheDesc>& out_atlas_cache_descs)
        {
            out_atlas_descs.clear();
            out_binding_descs.clear();
            out_atlas_cache_descs.clear();

            BakePublishedAtlasDesc atlas_desc{};
            atlas_desc.atlas_id = 0u;
            atlas_desc.width = atlas_result != nullptr ? atlas_result->atlas_resolution : config.atlas_resolution;
            atlas_desc.height = atlas_result != nullptr ? atlas_result->atlas_resolution : config.atlas_resolution;
            atlas_desc.relative_file_path = kDefaultAtlasRelativePath;
            atlas_desc.format = "rgba16f";
            atlas_desc.codec = "raw_half";
            atlas_desc.semantic = "diffuse_irradiance";
            out_atlas_descs.push_back(std::move(atlas_desc));

            out_atlas_cache_descs.reserve(out_atlas_descs.size());
            for (const BakePublishedAtlasDesc& published_atlas_desc : out_atlas_descs)
            {
                BakeAtlasCacheDesc cache_desc{};
                cache_desc.atlas_id = published_atlas_desc.atlas_id;
                cache_desc.texel_record_stride = sizeof(BakeTexelRecordDiskV1);
                cache_desc.texel_record_file = BuildCacheRelativePath("texel_records", published_atlas_desc.atlas_id, ".bin");
                cache_desc.accumulation_file = BuildCacheRelativePath("accum", published_atlas_desc.atlas_id, ".rgb32f.bin");
                cache_desc.sample_count_file = BuildCacheRelativePath("sample_count", published_atlas_desc.atlas_id, ".r32ui.bin");
                cache_desc.variance_file = BuildCacheRelativePath("variance", published_atlas_desc.atlas_id, ".r32f.bin");
                if (atlas_result != nullptr)
                {
                    for (const LightmapAtlasTexelRecord& texel_record : atlas_result->texel_records)
                    {
                        if (texel_record.atlas_id == published_atlas_desc.atlas_id)
                        {
                            ++cache_desc.texel_record_count;
                        }
                    }
                }

                out_atlas_cache_descs.push_back(std::move(cache_desc));
            }

            if (atlas_result != nullptr)
            {
                out_binding_descs.reserve(atlas_result->bindings.size());
                for (const LightmapAtlasBindingInfo& binding : atlas_result->bindings)
                {
                    BakePublishedBindingDesc binding_desc{};
                    binding_desc.atlas_id = binding.atlas_id;
                    binding_desc.primitive_hash = binding.primitive_hash;
                    binding_desc.has_node_key = true;
                    binding_desc.node_key = binding.stable_node_key;
                    binding_desc.scale_bias = binding.scale_bias;
                    out_binding_descs.push_back(std::move(binding_desc));
                }
            }
            else
            {
                out_binding_descs.reserve(import_result.primitive_instances.size());
                for (const BakePrimitiveImportInfo& primitive_info : import_result.primitive_instances)
                {
                    if (!primitive_info.can_emit_lightmap_binding)
                    {
                        continue;
                    }

                    BakePublishedBindingDesc binding_desc{};
                    binding_desc.atlas_id = 0u;
                    binding_desc.primitive_hash = primitive_info.primitive_hash;
                    binding_desc.has_node_key = true;
                    binding_desc.node_key = primitive_info.stable_node_key;
                    out_binding_descs.push_back(std::move(binding_desc));
                }
            }
        }

        bool EnsureDirectory(const std::filesystem::path& directory_path, const wchar_t* label, std::wstring& out_error);

        bool WriteJsonFile(const std::filesystem::path& file_path, const nlohmann::json& root, std::wstring& out_error)
        {
            std::ofstream stream(file_path, std::ios::out | std::ios::trunc | std::ios::binary);
            if (!stream.is_open())
            {
                out_error = L"Failed to open json output file: " + file_path.native();
                return false;
            }

            stream << root.dump(2) << "\n";
            if (!stream.good())
            {
                out_error = L"Failed to write json output file: " + file_path.native();
                return false;
            }

            return true;
        }

        bool EnsureDirectory(const std::filesystem::path& directory_path, const wchar_t* label, std::wstring& out_error)
        {
            std::error_code error_code{};
            std::filesystem::create_directories(directory_path, error_code);
            if (error_code)
            {
                out_error = std::wstring(L"Failed to create ") + label + L": " + directory_path.native();
                return false;
            }

            return true;
        }

        bool WriteZeroBytesFile(const std::filesystem::path& file_path,
                                std::uint64_t byte_count,
                                const wchar_t* label,
                                std::wstring& out_error)
        {
            if (!EnsureDirectory(file_path.parent_path(), label, out_error))
            {
                return false;
            }

            std::ofstream stream(file_path, std::ios::out | std::ios::trunc | std::ios::binary);
            if (!stream.is_open())
            {
                out_error = std::wstring(L"Failed to open ") + label + L": " + file_path.native();
                return false;
            }

            std::array<std::uint8_t, 4096> zero_chunk{};
            std::uint64_t bytes_remaining = byte_count;
            while (bytes_remaining > 0ull)
            {
                const std::uint64_t bytes_to_write =
                    std::min<std::uint64_t>(bytes_remaining, static_cast<std::uint64_t>(zero_chunk.size()));
                stream.write(reinterpret_cast<const char*>(zero_chunk.data()),
                             static_cast<std::streamsize>(bytes_to_write));
                if (!stream.good())
                {
                    out_error = std::wstring(L"Failed while writing ") + label + L": " + file_path.native();
                    return false;
                }

                bytes_remaining -= bytes_to_write;
            }

            return true;
        }
    }

    BakeOutputLayout BakeOutputWriter::BuildLayout(const std::filesystem::path& output_root)
    {
        BakeOutputLayout layout{};
        layout.root = output_root;
        layout.atlases = output_root / L"atlases";
        layout.debug = output_root / L"debug";
        layout.cache = output_root / L"cache";
        layout.manifest_path = output_root / L"manifest.json";
        layout.resume_path = output_root / std::filesystem::path(kResumeMetadataRelativePath);
        return layout;
    }

    bool BakeOutputWriter::EnsureLayout(const BakeOutputLayout& layout, std::wstring& out_error) const
    {
        return EnsureDirectory(layout.root, L"output root directory", out_error) &&
               EnsureDirectory(layout.atlases, L"atlas output directory", out_error) &&
               EnsureDirectory(layout.debug, L"debug output directory", out_error) &&
               EnsureDirectory(layout.cache, L"cache output directory", out_error);
    }

    bool BakeOutputWriter::WriteBootstrapPackage(const BakeJobConfig& config,
                                                 const BakeOutputLayout& layout,
                                                 const BakeSceneImportResult& import_result,
                                                 const LightmapAtlasBuildResult* atlas_result,
                                                 std::wstring& out_error) const
    {
        std::vector<BakePublishedAtlasDesc> atlas_descs{};
        std::vector<BakeAtlasCacheDesc> atlas_cache_descs{};
        std::vector<BakePublishedBindingDesc> binding_descs{};
        BuildPackageDescriptors(config, import_result, atlas_result, atlas_descs, binding_descs, atlas_cache_descs);

        for (const BakePublishedAtlasDesc& published_atlas_desc : atlas_descs)
        {
            if (!EnsurePlaceholderAtlas(layout, published_atlas_desc, out_error))
            {
                return false;
            }
        }

        if (atlas_result != nullptr &&
            !WriteAtlasCaches(layout, *atlas_result, atlas_cache_descs, out_error))
        {
            return false;
        }

        BakePackageProgressState progress_state{};
        progress_state.completed_samples = 0u;
        progress_state.has_accumulation_cache = !atlas_cache_descs.empty();
        progress_state.bootstrap_placeholder_payload = true;

        if (!WriteManifest(config, layout, import_result, atlas_result, atlas_descs, binding_descs, progress_state, out_error))
        {
            return false;
        }

        return WriteResumeMetadata(config,
                                   layout,
                                   import_result,
                                   atlas_descs,
                                   atlas_cache_descs,
                                   atlas_result,
                                   progress_state,
                                   out_error);
    }

    bool BakeOutputWriter::WriteImportSummary(const BakeSceneImportResult& import_result,
                                              const BakeOutputLayout& layout,
                                              std::wstring& out_error) const
    {
        nlohmann::json root{};
        root["schema_version"] = kManifestSchemaVersion;
        root["source_scene"] = ToJsonPathString(import_result.scene_path);
        root["load_succeeded"] = import_result.load_succeeded;
        root["node_count"] = import_result.node_count;
        root["mesh_count"] = import_result.mesh_count;
        root["instance_primitive_count"] = import_result.instance_primitive_count;
        root["valid_lightmap_primitive_count"] = import_result.valid_lightmap_primitive_count;
        root["validation_status"] = ResolveSceneValidationStatus(import_result);

        root["errors"] = nlohmann::json::array();
        for (const BakeSceneValidationMessage& message : import_result.errors)
        {
            root["errors"].push_back(ToJson(message));
        }

        root["warnings"] = nlohmann::json::array();
        for (const BakeSceneValidationMessage& message : import_result.warnings)
        {
            root["warnings"].push_back(ToJson(message));
        }

        root["primitives"] = nlohmann::json::array();
        for (const BakePrimitiveImportInfo& primitive : import_result.primitive_instances)
        {
            nlohmann::json primitive_json{
                {"node_key", primitive.stable_node_key},
                {"primitive_hash", primitive.primitive_hash},
                {"mesh_index", primitive.mesh_index},
                {"primitive_index", primitive.primitive_index},
                {"material_index", primitive.material_index},
                {"node_name", primitive.node_name},
                {"mesh_name", primitive.mesh_name},
                {"vertex_count", primitive.vertex_count},
                {"index_count", primitive.index_count},
                {"has_texcoord0", primitive.has_texcoord0},
                {"has_texcoord1", primitive.has_texcoord1},
                {"can_emit_lightmap_binding", primitive.can_emit_lightmap_binding},
                {"uv1_min", ToJson(primitive.uv1_min)},
                {"uv1_max", ToJson(primitive.uv1_max)},
                {"uv1_out_of_range_vertex_count", primitive.uv1_out_of_range_vertex_count},
                {"uv1_non_finite_vertex_count", primitive.uv1_non_finite_vertex_count},
                {"degenerate_uv_triangle_count", primitive.degenerate_uv_triangle_count},
                {"degenerate_position_triangle_count", primitive.degenerate_position_triangle_count},
                {"errors", nlohmann::json::array()},
                {"warnings", nlohmann::json::array()},
            };

            for (const BakeSceneValidationMessage& message : primitive.errors)
            {
                primitive_json["errors"].push_back(ToJson(message));
            }

            for (const BakeSceneValidationMessage& message : primitive.warnings)
            {
                primitive_json["warnings"].push_back(ToJson(message));
            }

            root["primitives"].push_back(std::move(primitive_json));
        }

        return WriteJsonFile(layout.root / std::filesystem::path(kImportSummaryRelativePath), root, out_error);
    }

    bool BakeOutputWriter::WriteAtlasSummary(const LightmapAtlasBuildResult& atlas_result,
                                             const BakeOutputLayout& layout,
                                             std::wstring& out_error) const
    {
        nlohmann::json root{};
        root["schema_version"] = kManifestSchemaVersion;
        root["atlas_resolution"] = atlas_result.atlas_resolution;
        root["texel_border"] = atlas_result.texel_border;
        root["binding_count"] = atlas_result.binding_count;
        root["texel_record_count"] = atlas_result.texel_record_count;
        root["overlapped_texel_count"] = atlas_result.overlapped_texel_count;
        root["validation_status"] = atlas_result.HasValidationErrors() ? "failed" : "passed";

        root["errors"] = nlohmann::json::array();
        for (const BakeSceneValidationMessage& message : atlas_result.errors)
        {
            root["errors"].push_back(ToJson(message));
        }

        root["warnings"] = nlohmann::json::array();
        for (const BakeSceneValidationMessage& message : atlas_result.warnings)
        {
            root["warnings"].push_back(ToJson(message));
        }

        root["bindings"] = nlohmann::json::array();
        for (const LightmapAtlasBindingInfo& binding : atlas_result.bindings)
        {
            nlohmann::json binding_json{
                {"atlas_id", binding.atlas_id},
                {"node_key", binding.stable_node_key},
                {"primitive_hash", binding.primitive_hash},
                {"scale_bias", ToJson(binding.scale_bias)},
                {"valid_texel_count", binding.valid_texel_count},
                {"overlap_texel_count", binding.overlap_texel_count},
                {"has_texel_bounds", binding.has_texel_bounds},
                {"node_name", binding.node_name},
                {"mesh_name", binding.mesh_name},
            };
            if (binding.has_texel_bounds)
            {
                binding_json["texel_min"] = nlohmann::json::array({binding.texel_min.x, binding.texel_min.y});
                binding_json["texel_max"] = nlohmann::json::array({binding.texel_max.x, binding.texel_max.y});
            }

            root["bindings"].push_back(std::move(binding_json));
        }

        return WriteJsonFile(layout.root / std::filesystem::path(kAtlasSummaryRelativePath), root, out_error);
    }

    bool BakeOutputWriter::RefreshPackageMetadata(const BakeJobConfig& config,
                                                  const BakeOutputLayout& layout,
                                                  const BakeSceneImportResult& import_result,
                                                  const LightmapAtlasBuildResult& atlas_result,
                                                  const BakePackageProgressState& progress_state,
                                                  std::wstring& out_error) const
    {
        std::vector<BakePublishedAtlasDesc> atlas_descs{};
        std::vector<BakePublishedBindingDesc> binding_descs{};
        std::vector<BakeAtlasCacheDesc> atlas_cache_descs{};
        BuildPackageDescriptors(config, import_result, &atlas_result, atlas_descs, binding_descs, atlas_cache_descs);

        BakePackageProgressState resolved_progress_state = progress_state;
        resolved_progress_state.has_accumulation_cache =
            resolved_progress_state.has_accumulation_cache || !atlas_cache_descs.empty();

        if (!WriteManifest(config,
                           layout,
                           import_result,
                           &atlas_result,
                           atlas_descs,
                           binding_descs,
                           resolved_progress_state,
                           out_error))
        {
            return false;
        }

        return WriteResumeMetadata(config,
                                   layout,
                                   import_result,
                                   atlas_descs,
                                   atlas_cache_descs,
                                   &atlas_result,
                                   resolved_progress_state,
                                   out_error);
    }

    bool BakeOutputWriter::WriteAtlasCaches(const BakeOutputLayout& layout,
                                            const LightmapAtlasBuildResult& atlas_result,
                                            const std::vector<BakeAtlasCacheDesc>& atlas_cache_descs,
                                            std::wstring& out_error) const
    {
        for (const BakeAtlasCacheDesc& cache_desc : atlas_cache_descs)
        {
            const std::filesystem::path texel_record_path = layout.root / std::filesystem::path(cache_desc.texel_record_file);
            if (!EnsureDirectory(texel_record_path.parent_path(), L"texel record cache directory", out_error))
            {
                return false;
            }

            std::ofstream texel_record_stream(texel_record_path, std::ios::out | std::ios::trunc | std::ios::binary);
            if (!texel_record_stream.is_open())
            {
                out_error = L"Failed to open texel record cache file: " + texel_record_path.native();
                return false;
            }

            unsigned written_texel_record_count = 0u;
            for (const LightmapAtlasTexelRecord& texel_record : atlas_result.texel_records)
            {
                if (texel_record.atlas_id != cache_desc.atlas_id)
                {
                    continue;
                }

                const BakeTexelRecordDiskV1 disk_record = ToDiskRecord(texel_record);
                texel_record_stream.write(reinterpret_cast<const char*>(&disk_record),
                                          static_cast<std::streamsize>(sizeof(BakeTexelRecordDiskV1)));
                if (!texel_record_stream.good())
                {
                    out_error = L"Failed while writing texel record cache file: " + texel_record_path.native();
                    return false;
                }

                ++written_texel_record_count;
            }

            if (written_texel_record_count != cache_desc.texel_record_count)
            {
                out_error = L"Texel record cache count mismatch for atlas cache file: " + texel_record_path.native();
                return false;
            }

            const std::uint64_t texel_count =
                static_cast<std::uint64_t>(atlas_result.atlas_resolution) *
                static_cast<std::uint64_t>(atlas_result.atlas_resolution);
            const std::filesystem::path accumulation_path = layout.root / std::filesystem::path(cache_desc.accumulation_file);
            if (!WriteZeroBytesFile(accumulation_path,
                                    texel_count * 3ull * sizeof(float),
                                    L"accumulation cache file",
                                    out_error))
            {
                return false;
            }

            const std::filesystem::path sample_count_path = layout.root / std::filesystem::path(cache_desc.sample_count_file);
            if (!WriteZeroBytesFile(sample_count_path,
                                    texel_count * sizeof(std::uint32_t),
                                    L"sample count cache file",
                                    out_error))
            {
                return false;
            }

            const std::filesystem::path variance_path = layout.root / std::filesystem::path(cache_desc.variance_file);
            if (!WriteZeroBytesFile(variance_path,
                                    texel_count * sizeof(float),
                                    L"variance cache file",
                                    out_error))
            {
                return false;
            }
        }

        return true;
    }

    bool BakeOutputWriter::EnsurePlaceholderAtlas(const BakeOutputLayout& layout,
                                                  const BakePublishedAtlasDesc& atlas_desc,
                                                  std::wstring& out_error) const
    {
        if (atlas_desc.width == 0u || atlas_desc.height == 0u)
        {
            out_error = L"Placeholder atlas dimensions must be non-zero.";
            return false;
        }

        const std::filesystem::path atlas_path = layout.root / std::filesystem::path(atlas_desc.relative_file_path);
        if (!EnsureDirectory(atlas_path.parent_path(), L"atlas parent directory", out_error))
        {
            return false;
        }

        const std::uint64_t pixel_count =
            static_cast<std::uint64_t>(atlas_desc.width) * static_cast<std::uint64_t>(atlas_desc.height);
        const std::uint64_t total_half_words = pixel_count * 4ull;
        const std::uint64_t expected_size = total_half_words * sizeof(std::uint16_t);

        std::error_code error_code{};
        if (std::filesystem::exists(atlas_path, error_code) && !error_code)
        {
            const std::uint64_t current_size = std::filesystem::file_size(atlas_path, error_code);
            if (!error_code && current_size == expected_size)
            {
                return true;
            }
        }

        std::ofstream stream(atlas_path, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!stream.is_open())
        {
            out_error = L"Failed to open placeholder atlas file: " + atlas_path.native();
            return false;
        }

        std::array<std::uint16_t, 4096> zero_chunk{};
        std::uint64_t words_remaining = total_half_words;
        while (words_remaining > 0ull)
        {
            const std::uint64_t words_to_write =
                std::min<std::uint64_t>(words_remaining, static_cast<std::uint64_t>(zero_chunk.size()));
            stream.write(reinterpret_cast<const char*>(zero_chunk.data()),
                         static_cast<std::streamsize>(words_to_write * sizeof(std::uint16_t)));
            if (!stream.good())
            {
                out_error = L"Failed while writing placeholder atlas file: " + atlas_path.native();
                return false;
            }

            words_remaining -= words_to_write;
        }

        return true;
    }

    bool BakeOutputWriter::WriteManifest(const BakeJobConfig& config,
                                         const BakeOutputLayout& layout,
                                         const BakeSceneImportResult& import_result,
                                         const LightmapAtlasBuildResult* atlas_result,
                                         const std::vector<BakePublishedAtlasDesc>& atlas_descs,
                                         const std::vector<BakePublishedBindingDesc>& binding_descs,
                                         const BakePackageProgressState& progress_state,
                                         std::wstring& out_error) const
    {
        const unsigned completed_samples = (std::min)(progress_state.completed_samples, config.target_samples);
        nlohmann::json root{};
        root["schema_version"] = kManifestSchemaVersion;
        root["source_scene"] = ToJsonPathString(config.scene_path);
        root["profile"] = kDefaultProfile;
        root["generator"] = {
            {"name", kGeneratorName},
            {"phase", kGeneratorPhase},
            {"timestamp_utc", BuildUtcTimestampString()},
        };
        root["integrator"] = {
            {"type", "path_tracing"},
            {"samples_per_iteration", config.samples_per_iteration},
            {"target_spp", config.target_samples},
            {"max_bounces", config.max_bounces},
            {"progressive", config.progressive},
        };
        root["output"] = {
            {"root", ToJsonPathString(layout.root)},
            {"cache_file", kResumeMetadataRelativePath},
            {"import_summary_file", kImportSummaryRelativePath},
            {"atlas_summary_file", kAtlasSummaryRelativePath},
            {"bootstrap_placeholder_payload", progress_state.bootstrap_placeholder_payload},
            {"atlas_cache_initialized", progress_state.has_accumulation_cache},
        };
        root["progress"] = {
            {"completed_samples", completed_samples},
            {"remaining_samples", config.target_samples - completed_samples},
            {"target_samples", config.target_samples},
            {"has_accumulation_cache", progress_state.has_accumulation_cache},
        };
        root["scene"] = {
            {"node_count", import_result.node_count},
            {"mesh_count", import_result.mesh_count},
            {"instance_primitive_count", import_result.instance_primitive_count},
            {"valid_lightmap_primitive_count", import_result.valid_lightmap_primitive_count},
        };
        if (atlas_result != nullptr)
        {
            root["scene"]["atlas_binding_count"] = atlas_result->binding_count;
            root["scene"]["atlas_texel_record_count"] = atlas_result->texel_record_count;
        }
        root["validation"] = {
            {"status", ResolvePackageValidationStatus(import_result, atlas_result)},
            {"scene_status", ResolveSceneValidationStatus(import_result)},
            {"atlas_status", ResolveAtlasValidationStatus(atlas_result)},
            {"error_count", CountPackageErrors(import_result, atlas_result)},
            {"warning_count", CountPackageWarnings(import_result, atlas_result)},
        };
        root["atlases"] = nlohmann::json::array();
        for (const BakePublishedAtlasDesc& atlas_desc : atlas_descs)
        {
            root["atlases"].push_back({
                {"id", atlas_desc.atlas_id},
                {"file", atlas_desc.relative_file_path},
                {"width", atlas_desc.width},
                {"height", atlas_desc.height},
                {"format", atlas_desc.format},
                {"codec", atlas_desc.codec},
                {"semantic", atlas_desc.semantic},
            });
        }

        root["bindings"] = nlohmann::json::array();
        for (const BakePublishedBindingDesc& binding_desc : binding_descs)
        {
            nlohmann::json binding_json{
                {"atlas_id", binding_desc.atlas_id},
                {"primitive_hash", binding_desc.primitive_hash},
                {"scale_bias", ToJson(binding_desc.scale_bias)},
            };
            if (binding_desc.has_node_key)
            {
                binding_json["node_key"] = binding_desc.node_key;
            }

            root["bindings"].push_back(std::move(binding_json));
        }

        return WriteJsonFile(layout.manifest_path, root, out_error);
    }

    bool BakeOutputWriter::WriteResumeMetadata(const BakeJobConfig& config,
                                               const BakeOutputLayout& layout,
                                               const BakeSceneImportResult& import_result,
                                               const std::vector<BakePublishedAtlasDesc>& atlas_descs,
                                               const std::vector<BakeAtlasCacheDesc>& atlas_cache_descs,
                                               const LightmapAtlasBuildResult* atlas_result,
                                               const BakePackageProgressState& progress_state,
                                               std::wstring& out_error) const
    {
        (void)layout;
        const unsigned completed_samples = (std::min)(progress_state.completed_samples, config.target_samples);
        const bool has_accumulation_cache =
            progress_state.has_accumulation_cache ||
            std::any_of(atlas_cache_descs.begin(),
                        atlas_cache_descs.end(),
                        [](const BakeAtlasCacheDesc& cache_desc)
                        {
                            return cache_desc.texel_record_count > 0u;
                        });

        nlohmann::json cache_root{};
        cache_root["schema_version"] = kManifestSchemaVersion;
        cache_root["source_scene"] = ToJsonPathString(config.scene_path);
        cache_root["generator"] = {
            {"name", kGeneratorName},
            {"phase", kGeneratorPhase},
            {"timestamp_utc", BuildUtcTimestampString()},
        };
        cache_root["job"] = {
            {"atlas_resolution", config.atlas_resolution},
            {"samples_per_iteration", config.samples_per_iteration},
            {"target_samples", config.target_samples},
            {"max_bounces", config.max_bounces},
            {"progressive", config.progressive},
            {"resume_requested", config.resume},
        };
        cache_root["progress"] = {
            {"completed_samples", completed_samples},
            {"target_samples", config.target_samples},
            {"has_accumulation_cache", has_accumulation_cache},
        };
        cache_root["scene"] = {
            {"node_count", import_result.node_count},
            {"mesh_count", import_result.mesh_count},
            {"instance_primitive_count", import_result.instance_primitive_count},
            {"valid_lightmap_primitive_count", import_result.valid_lightmap_primitive_count},
            {"validation_status", ResolvePackageValidationStatus(import_result, atlas_result)},
            {"scene_validation_status", ResolveSceneValidationStatus(import_result)},
            {"atlas_validation_status", ResolveAtlasValidationStatus(atlas_result)},
        };
        if (atlas_result != nullptr)
        {
            cache_root["atlas_build"] = {
                {"atlas_resolution", atlas_result->atlas_resolution},
                {"texel_border", atlas_result->texel_border},
                {"binding_count", atlas_result->binding_count},
                {"texel_record_count", atlas_result->texel_record_count},
                {"overlapped_texel_count", atlas_result->overlapped_texel_count},
            };
        }
        cache_root["published_atlases"] = nlohmann::json::array();
        for (const BakePublishedAtlasDesc& atlas_desc : atlas_descs)
        {
            cache_root["published_atlases"].push_back({
                {"atlas_id", atlas_desc.atlas_id},
                {"file", atlas_desc.relative_file_path},
                {"width", atlas_desc.width},
                {"height", atlas_desc.height},
            });
        }
        cache_root["cache_files"] = {
            {"manifest", "../manifest.json"},
            {"import_summary", "../debug/import_summary.json"},
            {"atlas_summary", "../debug/atlas_summary.json"},
            {"texel_records", nlohmann::json::array()},
            {"accumulation", nlohmann::json::array()},
            {"sample_count", nlohmann::json::array()},
            {"variance", nlohmann::json::array()},
        };
        cache_root["atlas_inputs"] = nlohmann::json::array();
        for (const BakeAtlasCacheDesc& cache_desc : atlas_cache_descs)
        {
            const std::filesystem::path texel_record_file = std::filesystem::path(cache_desc.texel_record_file).filename();
            const std::filesystem::path accumulation_file = std::filesystem::path(cache_desc.accumulation_file).filename();
            const std::filesystem::path sample_count_file = std::filesystem::path(cache_desc.sample_count_file).filename();
            const std::filesystem::path variance_file = std::filesystem::path(cache_desc.variance_file).filename();

            cache_root["cache_files"]["texel_records"].push_back(ToJsonPathString(texel_record_file));
            cache_root["cache_files"]["accumulation"].push_back(ToJsonPathString(accumulation_file));
            cache_root["cache_files"]["sample_count"].push_back(ToJsonPathString(sample_count_file));
            cache_root["cache_files"]["variance"].push_back(ToJsonPathString(variance_file));
            cache_root["atlas_inputs"].push_back({
                {"atlas_id", cache_desc.atlas_id},
                {"texel_record_file", ToJsonPathString(texel_record_file)},
                {"texel_record_count", cache_desc.texel_record_count},
                {"texel_record_stride", cache_desc.texel_record_stride},
                {"texel_record_format", cache_desc.texel_record_format},
                {"accumulation_file", ToJsonPathString(accumulation_file)},
                {"accumulation_format", cache_desc.accumulation_format},
                {"sample_count_file", ToJsonPathString(sample_count_file)},
                {"sample_count_format", cache_desc.sample_count_format},
                {"variance_file", ToJsonPathString(variance_file)},
                {"variance_format", cache_desc.variance_format},
            });
        }

        return WriteJsonFile(layout.resume_path, cache_root, out_error);
    }
}
