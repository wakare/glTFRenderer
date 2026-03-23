#pragma once

#include <filesystem>
#include <string>

namespace LightingBaker
{
    struct BakeJobConfig
    {
        std::filesystem::path scene_path;
        std::filesystem::path output_root;
        unsigned atlas_resolution = 1024;
        unsigned samples_per_iteration = 1;
        unsigned target_samples = 256;
        unsigned max_bounces = 4;
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
