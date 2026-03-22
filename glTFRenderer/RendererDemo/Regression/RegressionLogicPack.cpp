#include "RegressionLogicPack.h"

#include <algorithm>
#include <cctype>
#include <glm/glm/glm.hpp>

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

        bool ReadFloatVector3Or4(const nlohmann::json& value, glm::fvec4& out_value)
        {
            if (!value.is_array() || (value.size() != 3u && value.size() != 4u))
            {
                return false;
            }
            for (size_t index = 0; index < value.size(); ++index)
            {
                if (!value.at(index).is_number())
                {
                    return false;
                }
            }

            out_value.x = value.at(0).get<float>();
            out_value.y = value.at(1).get<float>();
            out_value.z = value.at(2).get<float>();
            if (value.size() == 4u)
            {
                out_value.w = value.at(3).get<float>();
            }
            return true;
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

        if (case_config.logic_pack == "model_viewer_lighting")
        {
            if (!context.lighting)
            {
                out_error = "model_viewer_lighting logic pack needs a valid lighting system.";
                return false;
            }

            const auto& args = case_config.logic_args;
            if (!args.is_null() && !args.is_object())
            {
                out_error = "logic_args must be an object for model_viewer_lighting logic pack.";
                return false;
            }

            auto global_params = context.lighting->GetGlobalParams();
            if (args.contains("environment_enabled"))
            {
                if (!args.at("environment_enabled").is_boolean())
                {
                    out_error = "logic_args.environment_enabled must be a boolean.";
                    return false;
                }
                global_params.environment_control.w = args.at("environment_enabled").get<bool>() ? 1.0f : 0.0f;
            }
            if (args.contains("use_environment_texture"))
            {
                if (!args.at("use_environment_texture").is_boolean())
                {
                    out_error = "logic_args.use_environment_texture must be a boolean.";
                    return false;
                }
                global_params.environment_texture_params.z = args.at("use_environment_texture").get<bool>() ? 1.0f : 0.0f;
            }
            if (args.contains("ibl_diffuse_intensity"))
            {
                if (!args.at("ibl_diffuse_intensity").is_number())
                {
                    out_error = "logic_args.ibl_diffuse_intensity must be a number.";
                    return false;
                }
                global_params.environment_control.x = args.at("ibl_diffuse_intensity").get<float>();
            }
            if (args.contains("ibl_specular_intensity"))
            {
                if (!args.at("ibl_specular_intensity").is_number())
                {
                    out_error = "logic_args.ibl_specular_intensity must be a number.";
                    return false;
                }
                global_params.environment_control.y = args.at("ibl_specular_intensity").get<float>();
            }
            if (args.contains("ibl_horizon_exponent"))
            {
                if (!args.at("ibl_horizon_exponent").is_number())
                {
                    out_error = "logic_args.ibl_horizon_exponent must be a number.";
                    return false;
                }
                global_params.environment_control.z = args.at("ibl_horizon_exponent").get<float>();
            }
            if (args.contains("environment_texture_intensity"))
            {
                if (!args.at("environment_texture_intensity").is_number())
                {
                    out_error = "logic_args.environment_texture_intensity must be a number.";
                    return false;
                }
                global_params.environment_texture_params.x = args.at("environment_texture_intensity").get<float>();
            }
            if (args.contains("sky_zenith_color"))
            {
                if (!ReadFloatVector3Or4(args.at("sky_zenith_color"), global_params.sky_zenith_color))
                {
                    out_error = "logic_args.sky_zenith_color must be a 3-element or 4-element number array.";
                    return false;
                }
            }
            if (args.contains("sky_horizon_color"))
            {
                if (!ReadFloatVector3Or4(args.at("sky_horizon_color"), global_params.sky_horizon_color))
                {
                    out_error = "logic_args.sky_horizon_color must be a 3-element or 4-element number array.";
                    return false;
                }
            }
            if (args.contains("ground_color"))
            {
                if (!ReadFloatVector3Or4(args.at("ground_color"), global_params.ground_color))
                {
                    out_error = "logic_args.ground_color must be a 3-element or 4-element number array.";
                    return false;
                }
            }

            context.lighting->SetGlobalParams(global_params);

            if (context.ssao)
            {
                auto ssao_global_params = context.ssao->GetGlobalParams();
                if (args.contains("ssao_enabled"))
                {
                    if (!args.at("ssao_enabled").is_boolean())
                    {
                        out_error = "logic_args.ssao_enabled must be a boolean.";
                        return false;
                    }
                    ssao_global_params.enabled = args.at("ssao_enabled").get<bool>() ? 1u : 0u;
                }
                if (args.contains("ssao_radius_world"))
                {
                    if (!args.at("ssao_radius_world").is_number())
                    {
                        out_error = "logic_args.ssao_radius_world must be a number.";
                        return false;
                    }
                    ssao_global_params.radius_world = args.at("ssao_radius_world").get<float>();
                }
                if (args.contains("ssao_intensity"))
                {
                    if (!args.at("ssao_intensity").is_number())
                    {
                        out_error = "logic_args.ssao_intensity must be a number.";
                        return false;
                    }
                    ssao_global_params.intensity = args.at("ssao_intensity").get<float>();
                }
                if (args.contains("ssao_power"))
                {
                    if (!args.at("ssao_power").is_number())
                    {
                        out_error = "logic_args.ssao_power must be a number.";
                        return false;
                    }
                    ssao_global_params.power = args.at("ssao_power").get<float>();
                }
                if (args.contains("ssao_bias"))
                {
                    if (!args.at("ssao_bias").is_number())
                    {
                        out_error = "logic_args.ssao_bias must be a number.";
                        return false;
                    }
                    ssao_global_params.bias = args.at("ssao_bias").get<float>();
                }
                if (args.contains("ssao_thickness"))
                {
                    if (!args.at("ssao_thickness").is_number())
                    {
                        out_error = "logic_args.ssao_thickness must be a number.";
                        return false;
                    }
                    ssao_global_params.thickness = args.at("ssao_thickness").get<float>();
                }
                if (args.contains("ssao_sample_distribution_power"))
                {
                    if (!args.at("ssao_sample_distribution_power").is_number())
                    {
                        out_error = "logic_args.ssao_sample_distribution_power must be a number.";
                        return false;
                    }
                    ssao_global_params.sample_distribution_power = args.at("ssao_sample_distribution_power").get<float>();
                }
                if (args.contains("ssao_blur_depth_reject"))
                {
                    if (!args.at("ssao_blur_depth_reject").is_number())
                    {
                        out_error = "logic_args.ssao_blur_depth_reject must be a number.";
                        return false;
                    }
                    ssao_global_params.blur_depth_reject = args.at("ssao_blur_depth_reject").get<float>();
                }
                if (args.contains("ssao_blur_normal_reject"))
                {
                    if (!args.at("ssao_blur_normal_reject").is_number())
                    {
                        out_error = "logic_args.ssao_blur_normal_reject must be a number.";
                        return false;
                    }
                    ssao_global_params.blur_normal_reject = args.at("ssao_blur_normal_reject").get<float>();
                }
                if (args.contains("ssao_sample_count"))
                {
                    if (!args.at("ssao_sample_count").is_number_unsigned())
                    {
                        out_error = "logic_args.ssao_sample_count must be an unsigned number.";
                        return false;
                    }
                    ssao_global_params.sample_count = args.at("ssao_sample_count").get<unsigned>();
                }
                if (args.contains("ssao_blur_radius"))
                {
                    if (!args.at("ssao_blur_radius").is_number_unsigned())
                    {
                        out_error = "logic_args.ssao_blur_radius must be an unsigned number.";
                        return false;
                    }
                    ssao_global_params.blur_radius = args.at("ssao_blur_radius").get<unsigned>();
                }
                context.ssao->SetGlobalParams(ssao_global_params);
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
