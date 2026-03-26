#pragma once

#include <filesystem>
#include <string>

namespace LightingBaker
{
    inline constexpr float BakeJobDefaultSkyIntensity = 1.0f;
    inline constexpr float BakeJobDefaultSkyGroundMix = 0.35f;

    struct BakeJobConfig
    {
        std::filesystem::path scene_path;
        std::filesystem::path output_root;
        unsigned atlas_resolution = 1024;
        unsigned samples_per_iteration = 1;
        unsigned target_samples = 256;
        unsigned max_bounces = 4;
        unsigned direct_light_samples = 1;
        unsigned environment_light_samples = 0;
        float sky_intensity = BakeJobDefaultSkyIntensity;
        float sky_ground_mix = BakeJobDefaultSkyGroundMix;
        bool progressive = true;
        bool resume = false;
        bool show_help = false;
    };

    struct CommandLineParseResult
    {
        BakeJobConfig config{};
        std::wstring error_message;
    };

    CommandLineParseResult ParseCommandLine(int argc, wchar_t* argv[]);
    std::wstring BuildUsageText();
}
