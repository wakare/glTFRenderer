#pragma once

#include "App/BakeJobConfig.h"

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace LightingBaker
{
    struct BakeSceneImportResult;
    struct BakeRayTracingDispatchRunResult;
    struct BakeRayTracingRuntimeBuildResult;
    struct LightmapAtlasBuildResult;
    struct BakeRayTracingSceneBuildResult;

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

    struct BakeAtlasCacheDesc
    {
        unsigned atlas_id{0};
        unsigned texel_record_count{0};
        unsigned texel_record_stride{0};
        std::string texel_record_file{};
        std::string texel_record_format{"lightmap_texel_record_v1"};
        std::string accumulation_file{};
        std::string accumulation_format{"rgb32f"};
        std::string sample_count_file{};
        std::string sample_count_format{"r32ui"};
        std::string variance_file{};
        std::string variance_format{"r32f"};
    };

    struct BakePackageProgressState
    {
        unsigned completed_samples{0};
        bool has_accumulation_cache{false};
        bool bootstrap_placeholder_payload{true};
    };

    class BakeOutputWriter
    {
    public:
        static BakeOutputLayout BuildLayout(const std::filesystem::path& output_root);

        bool EnsureLayout(const BakeOutputLayout& layout, std::wstring& out_error) const;
        bool WriteBootstrapPackage(const BakeJobConfig& config,
                                   const BakeOutputLayout& layout,
                                   const BakeSceneImportResult& import_result,
                                   const LightmapAtlasBuildResult* atlas_result,
                                   std::wstring& out_error) const;
        bool WriteImportSummary(const BakeSceneImportResult& import_result,
                                const BakeOutputLayout& layout,
                                std::wstring& out_error) const;
        bool WriteAtlasSummary(const LightmapAtlasBuildResult& atlas_result,
                               const BakeOutputLayout& layout,
                               std::wstring& out_error) const;
        bool WriteRayTracingSceneSummary(const BakeRayTracingSceneBuildResult& ray_tracing_scene,
                                        const BakeOutputLayout& layout,
                                        std::wstring& out_error) const;
        bool WriteRayTracingRuntimeSummary(const BakeRayTracingRuntimeBuildResult& ray_tracing_runtime,
                                          const BakeOutputLayout& layout,
                                          std::wstring& out_error) const;
        bool WriteRayTracingDispatchSummary(const BakeRayTracingDispatchRunResult& ray_tracing_dispatch,
                                            const BakeOutputLayout& layout,
                                            std::wstring& out_error) const;
        bool RefreshPackageMetadata(const BakeJobConfig& config,
                                    const BakeOutputLayout& layout,
                                    const BakeSceneImportResult& import_result,
                                    const LightmapAtlasBuildResult& atlas_result,
                                    const BakePackageProgressState& progress_state,
                                    std::wstring& out_error) const;

    private:
        bool EnsurePlaceholderAtlas(const BakeOutputLayout& layout,
                                    const BakePublishedAtlasDesc& atlas_desc,
                                    std::wstring& out_error) const;
        bool WriteManifest(const BakeJobConfig& config,
                           const BakeOutputLayout& layout,
                           const BakeSceneImportResult& import_result,
                           const LightmapAtlasBuildResult* atlas_result,
                           const std::vector<BakePublishedAtlasDesc>& atlas_descs,
                           const std::vector<BakePublishedBindingDesc>& binding_descs,
                           const BakePackageProgressState& progress_state,
                           std::wstring& out_error) const;
        bool WriteResumeMetadata(const BakeJobConfig& config,
                                 const BakeOutputLayout& layout,
                                 const BakeSceneImportResult& import_result,
                                 const std::vector<BakePublishedAtlasDesc>& atlas_descs,
                                 const std::vector<BakeAtlasCacheDesc>& atlas_cache_descs,
                                 const LightmapAtlasBuildResult* atlas_result,
                                 const BakePackageProgressState& progress_state,
                                 std::wstring& out_error) const;
        bool WriteAtlasCaches(const BakeOutputLayout& layout,
                              const LightmapAtlasBuildResult& atlas_result,
                              const std::vector<BakeAtlasCacheDesc>& atlas_cache_descs,
                              std::wstring& out_error) const;
    };
}
