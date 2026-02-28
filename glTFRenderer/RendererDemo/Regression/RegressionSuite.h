#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <glm/glm/glm.hpp>

#include "nlohmann_json/single_include/nlohmann/json.hpp"

namespace Regression
{
    struct CameraPose
    {
        bool enabled{false};
        glm::vec3 position{0.0f, 0.0f, 0.0f};
        glm::vec3 euler_angles{0.0f, 0.0f, 0.0f};
    };

    struct CaptureConfig
    {
        unsigned warmup_frames{180};
        unsigned capture_frames{1};
        bool capture_screenshot{true};
        bool capture_perf{true};
    };

    struct CaseConfig
    {
        std::string id{};
        CameraPose camera{};
        CaptureConfig capture{};
        std::string logic_pack{"none"};
        nlohmann::json logic_args{};
    };

    struct SuiteConfig
    {
        std::string suite_name{"unnamed_suite"};
        std::filesystem::path source_path{};
        bool disable_debug_ui{true};
        bool freeze_directional_light{true};
        bool disable_panel_input_state_machine{true};
        bool disable_prepass_animation{true};
        unsigned default_warmup_frames{180};
        unsigned default_capture_frames{1};
        std::vector<CaseConfig> cases{};
    };

    bool LoadSuiteConfig(const std::filesystem::path& suite_path, SuiteConfig& out_suite, std::string& out_error);
}
