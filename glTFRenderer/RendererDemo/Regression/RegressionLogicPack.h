#pragma once

#include <string>

#include "RegressionSuite.h"
#include "RendererSystem/RendererSystemFrostedGlass.h"

namespace Regression
{
    struct LogicPackContext
    {
        RendererSystemFrostedGlass* frosted_glass{nullptr};
        bool* enable_panel_input_state_machine{nullptr};
        bool* enable_frosted_prepass_feeds{nullptr};
        float* directional_light_speed_radians{nullptr};
    };

    bool ApplyLogicPack(const CaseConfig& case_config, const LogicPackContext& context, std::string& out_error);
}
