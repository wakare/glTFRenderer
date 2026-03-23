#pragma once

#include "App/BakeJobConfig.h"

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace LightingBaker
{
    struct BakeSceneImportResult;
    struct LightmapAtlasBuildResult;

    struct BakeOutputLayout
    {
        std::filesystem::path root;
        std::filesystem::path atlases;
        std::filesystem::path debug;
        std::filesystem::path cache;
        std::filesystem::path manifest_path;
        std::filesystem::path resume_path;
    };

    struct BakePublishedAtlasDesc
    {
        unsigned atlas_id{0};
        unsigned width{0};
        unsigned height{0};
        std::string relative_file_path{};
        std::string format{"rgba16f"};
        std::string codec{"raw_half"};
        std::string semantic{"diffuse_irradiance"};
    };

    struct BakePublishedBindingDesc
    {
        unsigned atlas_id{0};
        unsigned primitive_hash{0xffffffffu};
        bool has_node_key{false};
        unsigned node_key{0};
        std::array<float, 4> scale_bias{1.0f, 1.0f, 0.0f, 0.0f};
    };

    class BakeOutputWriter
    {
    public:
        static BakeOutputLayout BuildLayout(const std::filesystem::path& output_root);

        bool EnsureLayout(const BakeOutputLayout& layout, std::wstring& out_error) const;
        bool WriteBootstrapPackage(const BakeJobConfig& config,
                                   const BakeOutputLayout& layout,
                                   const BakeSceneImportResult& import_result,
                                   std::wstring& out_error) const;
        bool WriteImportSummary(const BakeSceneImportResult& import_result,
                                const BakeOutputLayout& layout,
                                std::wstring& out_error) const;
        bool WriteAtlasSummary(const LightmapAtlasBuildResult& atlas_result,
                               const BakeOutputLayout& layout,
                               std::wstring& out_error) const;

    private:
        bool EnsurePlaceholderAtlas(const BakeOutputLayout& layout,
                                    const BakePublishedAtlasDesc& atlas_desc,
                                    std::wstring& out_error) const;
        bool WriteManifest(const BakeJobConfig& config,
                           const BakeOutputLayout& layout,
                           const BakeSceneImportResult& import_result,
                           const std::vector<BakePublishedAtlasDesc>& atlas_descs,
                           const std::vector<BakePublishedBindingDesc>& binding_descs,
                           std::wstring& out_error) const;
        bool WriteResumeMetadata(const BakeJobConfig& config,
                                 const BakeOutputLayout& layout,
                                 const BakeSceneImportResult& import_result,
                                 const std::vector<BakePublishedAtlasDesc>& atlas_descs,
                                 std::wstring& out_error) const;
    };
}
