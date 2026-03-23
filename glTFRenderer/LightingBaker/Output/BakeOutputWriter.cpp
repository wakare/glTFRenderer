#include "BakeOutputWriter.h"

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
                                                 std::wstring& out_error) const
    {
        std::vector<BakePublishedAtlasDesc> atlas_descs{};
        BakePublishedAtlasDesc atlas_desc{};
        atlas_desc.atlas_id = 0u;
        atlas_desc.width = config.atlas_resolution;
        atlas_desc.height = config.atlas_resolution;
        atlas_desc.relative_file_path = kDefaultAtlasRelativePath;
        atlas_desc.format = "rgba16f";
        atlas_desc.codec = "raw_half";
        atlas_desc.semantic = "diffuse_irradiance";
        atlas_descs.push_back(std::move(atlas_desc));

        for (const BakePublishedAtlasDesc& atlas_desc : atlas_descs)
        {
            if (!EnsurePlaceholderAtlas(layout, atlas_desc, out_error))
            {
                return false;
            }
        }

        const std::vector<BakePublishedBindingDesc> binding_descs{};
        if (!WriteManifest(config, layout, atlas_descs, binding_descs, out_error))
        {
            return false;
        }

        if (!WriteResumeMetadata(config, layout, atlas_descs, out_error))
        {
            return false;
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
                                         const std::vector<BakePublishedAtlasDesc>& atlas_descs,
                                         const std::vector<BakePublishedBindingDesc>& binding_descs,
                                         std::wstring& out_error) const
    {
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
            {"bootstrap_placeholder_payload", true},
        };
        root["validation"] = {
            {"scene_import", "pending"},
            {"uv1", "pending"},
            {"bindings", "pending"},
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
                                               const std::vector<BakePublishedAtlasDesc>& atlas_descs,
                                               std::wstring& out_error) const
    {
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
            {"completed_samples", 0u},
            {"target_samples", config.target_samples},
            {"has_accumulation_cache", false},
        };
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
            {"accumulation", nlohmann::json::array()},
            {"sample_count", nlohmann::json::array()},
            {"variance", nlohmann::json::array()},
        };

        return WriteJsonFile(layout.resume_path, cache_root, out_error);
    }
}
