#include "RegressionSuite.h"

#include <fstream>
#include <sstream>

namespace Regression
{
    namespace
    {
        bool TryReadUnsigned(const nlohmann::json& root, const char* key, unsigned& out_value, std::string& out_error)
        {
            if (!root.contains(key))
            {
                return true;
            }
            const auto& value = root.at(key);
            if (!value.is_number_unsigned() && !value.is_number_integer())
            {
                out_error = std::string("Field '") + key + "' must be an integer.";
                return false;
            }
            const int int_value = value.get<int>();
            if (int_value < 0)
            {
                out_error = std::string("Field '") + key + "' must be >= 0.";
                return false;
            }
            out_value = static_cast<unsigned>(int_value);
            return true;
        }

        bool TryReadBool(const nlohmann::json& root, const char* key, bool& out_value, std::string& out_error)
        {
            if (!root.contains(key))
            {
                return true;
            }
            const auto& value = root.at(key);
            if (!value.is_boolean())
            {
                out_error = std::string("Field '") + key + "' must be a boolean.";
                return false;
            }
            out_value = value.get<bool>();
            return true;
        }

        bool TryReadString(const nlohmann::json& root, const char* key, std::string& out_value, std::string& out_error)
        {
            if (!root.contains(key))
            {
                return true;
            }
            const auto& value = root.at(key);
            if (!value.is_string())
            {
                out_error = std::string("Field '") + key + "' must be a string.";
                return false;
            }
            out_value = value.get<std::string>();
            return true;
        }

        bool TryReadVec3(const nlohmann::json& root, const char* key, glm::vec3& out_value, std::string& out_error)
        {
            if (!root.contains(key))
            {
                return true;
            }

            const auto& value = root.at(key);
            if (!value.is_array() || value.size() != 3u ||
                !value[0].is_number() || !value[1].is_number() || !value[2].is_number())
            {
                out_error = std::string("Field '") + key + "' must be an array of 3 numbers.";
                return false;
            }

            out_value = glm::vec3(value[0].get<float>(), value[1].get<float>(), value[2].get<float>());
            return true;
        }
    }

    bool LoadSuiteConfig(const std::filesystem::path& suite_path, SuiteConfig& out_suite, std::string& out_error)
    {
        out_suite = SuiteConfig{};
        out_error.clear();

        std::ifstream stream(suite_path, std::ios::in | std::ios::binary);
        if (!stream.is_open())
        {
            out_error = "Failed to open suite file: " + suite_path.string();
            return false;
        }

        std::stringstream buffer;
        buffer << stream.rdbuf();

        nlohmann::json root;
        try
        {
            root = nlohmann::json::parse(buffer.str());
        }
        catch (const std::exception& exception)
        {
            out_error = "Invalid suite json: " + std::string(exception.what());
            return false;
        }

        if (!root.is_object())
        {
            out_error = "Suite root must be a json object.";
            return false;
        }

        out_suite.source_path = std::filesystem::absolute(suite_path);
        out_suite.suite_name = suite_path.stem().string();
        TryReadString(root, "suite_name", out_suite.suite_name, out_error);
        if (!out_error.empty())
        {
            return false;
        }

        if (root.contains("global"))
        {
            const auto& global = root.at("global");
            if (!global.is_object())
            {
                out_error = "Field 'global' must be an object.";
                return false;
            }

            if (!TryReadBool(global, "disable_debug_ui", out_suite.disable_debug_ui, out_error) ||
                !TryReadBool(global, "freeze_directional_light", out_suite.freeze_directional_light, out_error) ||
                !TryReadBool(global, "disable_panel_input_state_machine", out_suite.disable_panel_input_state_machine, out_error) ||
                !TryReadBool(global, "disable_prepass_animation", out_suite.disable_prepass_animation, out_error) ||
                !TryReadUnsigned(global, "default_warmup_frames", out_suite.default_warmup_frames, out_error) ||
                !TryReadUnsigned(global, "default_capture_frames", out_suite.default_capture_frames, out_error) ||
                !TryReadBool(global, "default_capture_renderdoc", out_suite.default_capture_renderdoc, out_error) ||
                !TryReadUnsigned(global, "default_renderdoc_capture_frame_offset", out_suite.default_renderdoc_capture_frame_offset, out_error) ||
                !TryReadBool(global, "default_keep_renderdoc_on_success", out_suite.default_keep_renderdoc_on_success, out_error) ||
                !TryReadBool(global, "default_capture_pix", out_suite.default_capture_pix, out_error) ||
                !TryReadUnsigned(global, "default_pix_capture_frame_offset", out_suite.default_pix_capture_frame_offset, out_error) ||
                !TryReadBool(global, "default_keep_pix_on_success", out_suite.default_keep_pix_on_success, out_error))
            {
                return false;
            }
        }

        if (!root.contains("cases") || !root.at("cases").is_array())
        {
            out_error = "Field 'cases' must exist and be an array.";
            return false;
        }

        const auto& cases = root.at("cases");
        if (cases.empty())
        {
            out_error = "Field 'cases' cannot be empty.";
            return false;
        }

        out_suite.cases.reserve(cases.size());
        for (size_t index = 0; index < cases.size(); ++index)
        {
            const auto& json_case = cases[index];
            if (!json_case.is_object())
            {
                out_error = "Case entry must be an object.";
                return false;
            }

            CaseConfig case_config{};
            case_config.id = "case_" + std::to_string(index + 1u);
            case_config.capture.warmup_frames = out_suite.default_warmup_frames;
            case_config.capture.capture_frames = out_suite.default_capture_frames;
            case_config.capture.capture_renderdoc = out_suite.default_capture_renderdoc;
            case_config.capture.renderdoc_capture_frame_offset = out_suite.default_renderdoc_capture_frame_offset;
            case_config.capture.keep_renderdoc_on_success = out_suite.default_keep_renderdoc_on_success;
            case_config.capture.capture_pix = out_suite.default_capture_pix;
            case_config.capture.pix_capture_frame_offset = out_suite.default_pix_capture_frame_offset;
            case_config.capture.keep_pix_on_success = out_suite.default_keep_pix_on_success;

            if (!TryReadString(json_case, "id", case_config.id, out_error))
            {
                return false;
            }

            if (json_case.contains("capture"))
            {
                const auto& capture = json_case.at("capture");
                if (!capture.is_object())
                {
                    out_error = "Field 'capture' must be an object.";
                    return false;
                }

                if (!TryReadUnsigned(capture, "warmup_frames", case_config.capture.warmup_frames, out_error) ||
                    !TryReadUnsigned(capture, "capture_frames", case_config.capture.capture_frames, out_error) ||
                    !TryReadBool(capture, "capture_screenshot", case_config.capture.capture_screenshot, out_error) ||
                    !TryReadBool(capture, "capture_perf", case_config.capture.capture_perf, out_error) ||
                    !TryReadBool(capture, "capture_renderdoc", case_config.capture.capture_renderdoc, out_error) ||
                    !TryReadUnsigned(capture, "renderdoc_capture_frame_offset", case_config.capture.renderdoc_capture_frame_offset, out_error) ||
                    !TryReadBool(capture, "keep_renderdoc_on_success", case_config.capture.keep_renderdoc_on_success, out_error) ||
                    !TryReadBool(capture, "capture_pix", case_config.capture.capture_pix, out_error) ||
                    !TryReadUnsigned(capture, "pix_capture_frame_offset", case_config.capture.pix_capture_frame_offset, out_error) ||
                    !TryReadBool(capture, "keep_pix_on_success", case_config.capture.keep_pix_on_success, out_error))
                {
                    return false;
                }
            }

            if (!TryReadUnsigned(json_case, "warmup_frames", case_config.capture.warmup_frames, out_error) ||
                !TryReadUnsigned(json_case, "capture_frames", case_config.capture.capture_frames, out_error) ||
                !TryReadBool(json_case, "capture_screenshot", case_config.capture.capture_screenshot, out_error) ||
                !TryReadBool(json_case, "capture_perf", case_config.capture.capture_perf, out_error) ||
                !TryReadBool(json_case, "capture_renderdoc", case_config.capture.capture_renderdoc, out_error) ||
                !TryReadUnsigned(json_case, "renderdoc_capture_frame_offset", case_config.capture.renderdoc_capture_frame_offset, out_error) ||
                !TryReadBool(json_case, "keep_renderdoc_on_success", case_config.capture.keep_renderdoc_on_success, out_error) ||
                !TryReadBool(json_case, "capture_pix", case_config.capture.capture_pix, out_error) ||
                !TryReadUnsigned(json_case, "pix_capture_frame_offset", case_config.capture.pix_capture_frame_offset, out_error) ||
                !TryReadBool(json_case, "keep_pix_on_success", case_config.capture.keep_pix_on_success, out_error))
            {
                return false;
            }

            if (json_case.contains("camera"))
            {
                const auto& camera = json_case.at("camera");
                if (!camera.is_object())
                {
                    out_error = "Field 'camera' must be an object.";
                    return false;
                }

                case_config.camera.enabled = true;
                if (!TryReadVec3(camera, "position", case_config.camera.position, out_error) ||
                    !TryReadVec3(camera, "euler_angles", case_config.camera.euler_angles, out_error))
                {
                    return false;
                }

                if (camera.contains("euler_radians") &&
                    !TryReadVec3(camera, "euler_radians", case_config.camera.euler_angles, out_error))
                {
                    return false;
                }
            }

            if (!TryReadString(json_case, "logic_pack", case_config.logic_pack, out_error))
            {
                return false;
            }
            if (json_case.contains("logic_args"))
            {
                case_config.logic_args = json_case.at("logic_args");
            }
            else if (json_case.contains("logic"))
            {
                case_config.logic_args = json_case.at("logic");
            }

            if (case_config.id.empty())
            {
                out_error = "Case id cannot be empty.";
                return false;
            }
            if (case_config.capture.capture_frames == 0u)
            {
                case_config.capture.capture_frames = 1u;
            }

            out_suite.cases.push_back(std::move(case_config));
        }

        return true;
    }
}
