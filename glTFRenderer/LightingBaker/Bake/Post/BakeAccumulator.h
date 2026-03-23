#pragma once

#include "App/BakeJobConfig.h"

#include <filesystem>
#include <string>
#include <vector>

namespace LightingBaker
{
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

    struct BakeAccumulatorResumeState
    {
        std::filesystem::path resume_path{};
        std::filesystem::path cache_root{};
        std::filesystem::path source_scene{};
        unsigned atlas_resolution{0};
        unsigned samples_per_iteration{0};
        unsigned target_samples{0};
        unsigned max_bounces{0};
        unsigned completed_samples{0};
        bool progressive{true};
        bool has_accumulation_cache{false};
        std::vector<BakeAccumulatorAtlasInputState> atlas_inputs{};
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
    };
}
