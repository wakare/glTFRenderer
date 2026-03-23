#pragma once

#include "App/BakeJobConfig.h"

#include <filesystem>
#include <string>
#include <vector>

namespace LightingBaker
{
    struct BakeRayTracingDispatchRunResult;

    struct BakeAccumulatorSettings
    {
        unsigned samples_per_iteration = 1;
        unsigned target_samples = 256;
        bool progressive = true;
    };

    struct LightmapAtlasBuildResult;

    struct BakeAccumulatorAtlasInputState
    {
        unsigned atlas_id{0};
        unsigned texel_record_count{0};
        unsigned texel_record_stride{0};
        std::filesystem::path texel_record_file{};
        std::filesystem::path accumulation_file{};
        std::filesystem::path sample_count_file{};
        std::filesystem::path variance_file{};
    };

    struct BakeAccumulatorPublishedAtlasState
    {
        unsigned atlas_id{0};
        unsigned width{0};
        unsigned height{0};
        std::filesystem::path atlas_file{};
    };

    struct BakeAccumulatorResumeState
    {
        std::filesystem::path resume_path{};
        std::filesystem::path cache_root{};
        std::filesystem::path package_root{};
        std::filesystem::path source_scene{};
        unsigned atlas_resolution{0};
        unsigned samples_per_iteration{0};
        unsigned target_samples{0};
        unsigned max_bounces{0};
        unsigned completed_samples{0};
        bool progressive{true};
        bool has_accumulation_cache{false};
        std::vector<BakeAccumulatorAtlasInputState> atlas_inputs{};
        std::vector<BakeAccumulatorPublishedAtlasState> published_atlases{};
    };

    struct BakeAccumulationRunResult
    {
        unsigned previous_completed_samples{0};
        unsigned added_samples{0};
        unsigned completed_samples{0};
        unsigned target_samples{0};
        bool cache_files_updated{false};
        bool published_atlases_updated{false};
    };

    class BakeAccumulator
    {
    public:
        BakeAccumulator() = default;

        bool LoadResumeState(const std::filesystem::path& resume_path,
                             BakeAccumulatorResumeState& out_state,
                             std::wstring& out_error) const;
        bool ValidateResumeState(const BakeAccumulatorResumeState& resume_state,
                                 const BakeJobConfig& config,
                                 const LightmapAtlasBuildResult& atlas_result,
                                 std::wstring& out_error) const;
        bool AccumulateRayTracingDispatchOutput(const BakeAccumulatorResumeState& resume_state,
                                               const BakeJobConfig& config,
                                               const LightmapAtlasBuildResult& atlas_result,
                                               const BakeRayTracingDispatchRunResult& ray_tracing_dispatch,
                                               BakeAccumulationRunResult& out_result,
                                               std::wstring& out_error) const;
        bool AccumulateDebugHemispherePreview(const BakeAccumulatorResumeState& resume_state,
                                              const BakeJobConfig& config,
                                              const LightmapAtlasBuildResult& atlas_result,
                                              BakeAccumulationRunResult& out_result,
                                              std::wstring& out_error) const;
    };
}
