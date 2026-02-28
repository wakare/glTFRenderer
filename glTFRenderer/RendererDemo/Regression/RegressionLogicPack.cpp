#include "RegressionLogicPack.h"

#include <algorithm>
#include <cctype>

namespace Regression
{
    namespace
    {
        bool ParseBlurSourceMode(const std::string& mode_name, RendererSystemFrostedGlass::BlurSourceMode& out_mode)
        {
            std::string normalized = mode_name;
            std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });

            if (normalized == "legacy" || normalized == "legacy_pyramid")
            {
                out_mode = RendererSystemFrostedGlass::BlurSourceMode::LegacyPyramid;
                return true;
            }
            if (normalized == "shared_mip" || normalized == "sharedmip")
            {
                out_mode = RendererSystemFrostedGlass::BlurSourceMode::SharedMip;
                return true;
            }
            if (normalized == "shared_dual" || normalized == "shareddual")
            {
                out_mode = RendererSystemFrostedGlass::BlurSourceMode::SharedDual;
                return true;
            }
            return false;
        }
    }

    bool ApplyLogicPack(const CaseConfig& case_config, const LogicPackContext& context, std::string& out_error)
    {
        out_error.clear();

        if (case_config.logic_pack.empty() ||
            case_config.logic_pack == "none")
        {
            return true;
        }

        if (case_config.logic_pack != "frosted_glass")
        {
            out_error = "Unsupported logic pack: " + case_config.logic_pack;
            return false;
        }

        if (!context.frosted_glass)
        {
            out_error = "frosted_glass logic pack needs a valid frosted glass system.";
            return false;
        }

        const auto& args = case_config.logic_args;
        if (!args.is_null() && !args.is_object())
        {
            out_error = "logic_args must be an object for frosted_glass logic pack.";
            return false;
        }

        if (args.contains("blur_source_mode"))
        {
            if (!args.at("blur_source_mode").is_string())
            {
                out_error = "logic_args.blur_source_mode must be a string.";
                return false;
            }

            RendererSystemFrostedGlass::BlurSourceMode mode{};
            if (!ParseBlurSourceMode(args.at("blur_source_mode").get<std::string>(), mode))
            {
                out_error = "logic_args.blur_source_mode is invalid.";
                return false;
            }
            context.frosted_glass->SetBlurSourceMode(mode);
        }

        if (args.contains("full_fog_mode"))
        {
            if (!args.at("full_fog_mode").is_boolean())
            {
                out_error = "logic_args.full_fog_mode must be a boolean.";
                return false;
            }
            context.frosted_glass->SetFullFogMode(args.at("full_fog_mode").get<bool>());
        }

        bool reset_temporal_history = true;
        if (args.contains("reset_temporal_history"))
        {
            if (!args.at("reset_temporal_history").is_boolean())
            {
                out_error = "logic_args.reset_temporal_history must be a boolean.";
                return false;
            }
            reset_temporal_history = args.at("reset_temporal_history").get<bool>();
        }
        if (reset_temporal_history)
        {
            context.frosted_glass->ForceResetTemporalHistory();
        }

        if (args.contains("enable_panel_input_state_machine") && context.enable_panel_input_state_machine)
        {
            if (!args.at("enable_panel_input_state_machine").is_boolean())
            {
                out_error = "logic_args.enable_panel_input_state_machine must be a boolean.";
                return false;
            }
            *context.enable_panel_input_state_machine = args.at("enable_panel_input_state_machine").get<bool>();
        }

        if (args.contains("enable_frosted_prepass_feeds") && context.enable_frosted_prepass_feeds)
        {
            if (!args.at("enable_frosted_prepass_feeds").is_boolean())
            {
                out_error = "logic_args.enable_frosted_prepass_feeds must be a boolean.";
                return false;
            }
            *context.enable_frosted_prepass_feeds = args.at("enable_frosted_prepass_feeds").get<bool>();
        }

        if (args.contains("directional_light_speed_radians") && context.directional_light_speed_radians)
        {
            if (!args.at("directional_light_speed_radians").is_number())
            {
                out_error = "logic_args.directional_light_speed_radians must be a number.";
                return false;
            }
            *context.directional_light_speed_radians = args.at("directional_light_speed_radians").get<float>();
        }

        return true;
    }
}
