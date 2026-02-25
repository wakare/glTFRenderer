#include "RendererSystemFrostedGlass.h"
#include <algorithm>
#include <cmath>
#include <imgui/imgui.h>

namespace
{
    constexpr unsigned MULTILAYER_MODE_SINGLE = 0;
    constexpr unsigned MULTILAYER_MODE_AUTO = 1;
    constexpr unsigned MULTILAYER_MODE_FORCE = 2;
    constexpr unsigned MULTILAYER_AUTO_OVER_BUDGET_TRIGGER_FRAMES = 6;
    constexpr unsigned MULTILAYER_AUTO_COOLDOWN_FRAMES = 180;
}

RendererSystemFrostedGlass::RendererSystemFrostedGlass(std::shared_ptr<RendererSystemSceneRenderer> scene,
                                                       std::shared_ptr<RendererSystemLighting> lighting)
    : m_scene(std::move(scene))
    , m_lighting(std::move(lighting))
{
}

unsigned RendererSystemFrostedGlass::AddPanel(const FrostedGlassPanelDesc& panel_desc)
{
    GLTF_CHECK(m_panel_descs.size() < MAX_PANEL_COUNT);
    const unsigned index = static_cast<unsigned>(m_panel_descs.size());
    m_panel_descs.push_back(panel_desc);
    PanelRuntimeState runtime_state{};
    runtime_state.state_weights.fill(0.0f);
    runtime_state.target_state = panel_desc.interaction_state;
    runtime_state.state_weights[ToInteractionStateIndex(panel_desc.interaction_state)] = 1.0f;
    m_panel_runtime_states.push_back(runtime_state);
    m_need_upload_panels = true;
    m_need_upload_global_params = true;
    return index;
}

bool RendererSystemFrostedGlass::UpdatePanel(unsigned index, const FrostedGlassPanelDesc& panel_desc)
{
    GLTF_CHECK(ContainsPanel(index));
    m_panel_descs[index] = panel_desc;
    if (index < m_panel_runtime_states.size())
    {
        m_panel_runtime_states[index].target_state = panel_desc.interaction_state;
    }
    m_need_upload_panels = true;
    return true;
}

bool RendererSystemFrostedGlass::ContainsPanel(unsigned index) const
{
    return index < m_panel_descs.size();
}

bool RendererSystemFrostedGlass::SetPanelInteractionState(unsigned index, PanelInteractionState interaction_state)
{
    GLTF_CHECK(ContainsPanel(index));
    GLTF_CHECK(index < m_panel_runtime_states.size());
    if (m_panel_descs[index].interaction_state == interaction_state &&
        m_panel_runtime_states[index].target_state == interaction_state)
    {
        return true;
    }
    m_panel_descs[index].interaction_state = interaction_state;
    m_panel_runtime_states[index].target_state = interaction_state;
    m_need_upload_panels = true;
    return true;
}

RendererSystemFrostedGlass::PanelInteractionState RendererSystemFrostedGlass::GetPanelInteractionState(unsigned index) const
{
    GLTF_CHECK(ContainsPanel(index));
    GLTF_CHECK(index < m_panel_runtime_states.size());
    return m_panel_runtime_states[index].target_state;
}

bool RendererSystemFrostedGlass::Init(RendererInterface::ResourceOperator& resource_operator,
                                      RendererInterface::RenderGraph& graph)
{
    GLTF_CHECK(m_scene);
    GLTF_CHECK(m_lighting);
    GLTF_CHECK(m_scene->HasInit());
    GLTF_CHECK(m_lighting->HasInit());
    m_temporal_history_read_is_a = true;
    m_temporal_force_reset = true;
    m_temporal_history_valid = false;
    m_global_params.temporal_history_valid = 0;
    m_multilayer_runtime_enabled = m_global_params.multilayer_mode != MULTILAYER_MODE_SINGLE;
    m_multilayer_over_budget_streak = 0;
    m_multilayer_cooldown_frames = 0;
    m_global_params.multilayer_runtime_enabled = m_multilayer_runtime_enabled ? 1u : 0u;
    m_need_upload_global_params = true;

    const unsigned width = (std::max)(1u, resource_operator.GetCurrentRenderWidth());
    const unsigned height = (std::max)(1u, resource_operator.GetCurrentRenderHeight());
    const unsigned half_width = (width + 1u) / 2u;
    const unsigned half_height = (height + 1u) / 2u;
    const unsigned quarter_width = (width + 3u) / 4u;
    const unsigned quarter_height = (height + 3u) / 4u;
    const unsigned eighth_width = (width + 7u) / 8u;
    const unsigned eighth_height = (height + 7u) / 8u;
    const unsigned sixteenth_width = (width + 15u) / 16u;
    const unsigned sixteenth_height = (height + 15u) / 16u;
    const unsigned thirtysecond_width = (width + 31u) / 32u;
    const unsigned thirtysecond_height = (height + 31u) / 32u;
    const auto postfx_usage = static_cast<RendererInterface::ResourceUsage>(
        RendererInterface::ResourceUsage::RENDER_TARGET |
        RendererInterface::ResourceUsage::COPY_SRC |
        RendererInterface::ResourceUsage::SHADER_RESOURCE |
        RendererInterface::ResourceUsage::UNORDER_ACCESS);
    const auto make_compute_dispatch = [](unsigned dispatch_width, unsigned dispatch_height) -> RendererInterface::RenderExecuteCommand
    {
        RendererInterface::RenderExecuteCommand dispatch_command{};
        dispatch_command.type = RendererInterface::ExecuteCommandType::COMPUTE_DISPATCH_COMMAND;
        dispatch_command.parameter.dispatch_parameter.group_size_x = (dispatch_width + 7) / 8;
        dispatch_command.parameter.dispatch_parameter.group_size_y = (dispatch_height + 7) / 8;
        dispatch_command.parameter.dispatch_parameter.group_size_z = 1;
        return dispatch_command;
    };

    GLTF_CHECK(m_postfx_shared_resources.Init(
        resource_operator,
        "PostFX_Shared",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage));

    m_frosted_pass_output = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_Output",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage);
    m_frosted_back_composite_output = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_BackComposite",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage);
    m_frosted_mask_parameter_output = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_MaskParameter",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage);
    m_frosted_mask_parameter_secondary_output = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_MaskParameter_Secondary",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage);
    m_frosted_panel_optics_output = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_PanelOptics",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage);
    m_frosted_panel_optics_secondary_output = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_PanelOptics_Secondary",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage);
    m_frosted_panel_profile_output = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_PanelProfile",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage);
    m_frosted_panel_profile_secondary_output = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_PanelProfile_Secondary",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage);
    m_frosted_panel_payload_depth = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_PanelPayloadDepth",
        RendererInterface::D32,
        RendererInterface::default_clear_depth,
        static_cast<RendererInterface::ResourceUsage>(RendererInterface::ResourceUsage::DEPTH_STENCIL));
    m_frosted_panel_payload_depth_secondary = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_PanelPayloadDepth_Secondary",
        RendererInterface::D32,
        RendererInterface::default_clear_depth,
        static_cast<RendererInterface::ResourceUsage>(RendererInterface::ResourceUsage::DEPTH_STENCIL));
    m_half_multilayer_ping = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_Half_Ping",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.5f,
        0.5f,
        1,
        1);
    m_half_multilayer_pong = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_Half_Pong",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.5f,
        0.5f,
        1,
        1);
    m_quarter_multilayer_ping = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_Quarter_Ping",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.25f,
        0.25f,
        1,
        1);
    m_quarter_multilayer_pong = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_Quarter_Pong",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.25f,
        0.25f,
        1,
        1);
    m_eighth_blur_ping = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_Eighth_Ping",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.125f,
        0.125f,
        1,
        1);
    m_eighth_blur_pong = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_Eighth_Pong",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.125f,
        0.125f,
        1,
        1);
    m_sixteenth_blur_ping = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_Sixteenth_Ping",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.0625f,
        0.0625f,
        1,
        1);
    m_sixteenth_blur_pong = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_Sixteenth_Pong",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.0625f,
        0.0625f,
        1,
        1);
    m_thirtysecond_blur_ping = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_ThirtySecond_Ping",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.03125f,
        0.03125f,
        1,
        1);
    m_thirtysecond_blur_pong = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_ThirtySecond_Pong",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.03125f,
        0.03125f,
        1,
        1);
    m_eighth_multilayer_ping = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_Eighth_Ping",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.125f,
        0.125f,
        1,
        1);
    m_eighth_multilayer_pong = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_Eighth_Pong",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.125f,
        0.125f,
        1,
        1);
    m_sixteenth_multilayer_ping = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_Sixteenth_Ping",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.0625f,
        0.0625f,
        1,
        1);
    m_sixteenth_multilayer_pong = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_Sixteenth_Pong",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.0625f,
        0.0625f,
        1,
        1);
    m_thirtysecond_multilayer_ping = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_ThirtySecond_Ping",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.03125f,
        0.03125f,
        1,
        1);
    m_thirtysecond_multilayer_pong = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_ThirtySecond_Pong",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.03125f,
        0.03125f,
        1,
        1);
    m_half_blur_final_output = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_Half_BlurFinal",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.5f,
        0.5f,
        1,
        1);
    m_quarter_blur_final_output = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_Quarter_BlurFinal",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.25f,
        0.25f,
        1,
        1);
    m_eighth_blur_final_output = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_Eighth_BlurFinal",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.125f,
        0.125f,
        1,
        1);
    m_sixteenth_blur_final_output = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_Sixteenth_BlurFinal",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.0625f,
        0.0625f,
        1,
        1);
    m_thirtysecond_blur_final_output = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_ThirtySecond_BlurFinal",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.03125f,
        0.03125f,
        1,
        1);
    m_half_multilayer_blur_final_output = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_Half_BlurFinal",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.5f,
        0.5f,
        1,
        1);
    m_quarter_multilayer_blur_final_output = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_Quarter_BlurFinal",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.25f,
        0.25f,
        1,
        1);
    m_eighth_multilayer_blur_final_output = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_Eighth_BlurFinal",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.125f,
        0.125f,
        1,
        1);
    m_sixteenth_multilayer_blur_final_output = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_Sixteenth_BlurFinal",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.0625f,
        0.0625f,
        1,
        1);
    m_thirtysecond_multilayer_blur_final_output = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_ThirtySecond_BlurFinal",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage,
        0.03125f,
        0.03125f,
        1,
        1);
    m_temporal_history_a = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_History_A",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage);
    m_temporal_history_b = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_History_B",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        postfx_usage);

    const RendererInterface::RenderTargetHandle half_ping = GetHalfResPing();
    const RendererInterface::RenderTargetHandle half_pong = GetHalfResPong();
    const RendererInterface::RenderTargetHandle quarter_ping = GetQuarterResPing();
    const RendererInterface::RenderTargetHandle quarter_pong = GetQuarterResPong();

    RendererInterface::BufferDesc panel_data_buffer_desc{};
    panel_data_buffer_desc.name = "g_frosted_panels";
    panel_data_buffer_desc.size = sizeof(FrostedGlassPanelGpuData) * MAX_PANEL_COUNT;
    panel_data_buffer_desc.type = RendererInterface::DEFAULT;
    panel_data_buffer_desc.usage = RendererInterface::USAGE_SRV;
    m_frosted_panel_data_handle = resource_operator.CreateBuffer(panel_data_buffer_desc);

    RendererInterface::BufferDesc global_params_buffer_desc{};
    global_params_buffer_desc.name = "FrostedGlassGlobalBuffer";
    global_params_buffer_desc.size = sizeof(FrostedGlassGlobalParams);
    global_params_buffer_desc.type = RendererInterface::DEFAULT;
    global_params_buffer_desc.usage = RendererInterface::USAGE_CBV;
    m_frosted_global_params_handle = resource_operator.CreateBuffer(global_params_buffer_desc);

    RendererSystemOutput<RendererSystemSceneRenderer> scene_output;

    RendererInterface::RenderGraph::RenderPassSetupInfo downsample_half_pass_setup_info{};
    downsample_half_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    downsample_half_pass_setup_info.debug_group = "Frosted Glass";
    downsample_half_pass_setup_info.debug_name = "Downsample Half";
    downsample_half_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "DownsampleMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_lighting->GetLightingOutput()};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {half_ping};

        downsample_half_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    downsample_half_pass_setup_info.execute_command = make_compute_dispatch(half_width, half_height);
    m_downsample_half_pass_node = graph.CreateRenderGraphNode(resource_operator, downsample_half_pass_setup_info);

    RendererInterface::BufferBindingDesc global_params_binding_desc{};
    global_params_binding_desc.binding_type = RendererInterface::BufferBindingDesc::CBV;
    global_params_binding_desc.buffer_handle = m_frosted_global_params_handle;
    global_params_binding_desc.is_structured_buffer = false;

    RendererInterface::RenderGraph::RenderPassSetupInfo blur_half_horizontal_pass_setup_info{};
    blur_half_horizontal_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    blur_half_horizontal_pass_setup_info.debug_group = "Frosted Glass";
    blur_half_horizontal_pass_setup_info.debug_name = "Blur Half Horizontal";
    blur_half_horizontal_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "BlurHorizontalMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    blur_half_horizontal_pass_setup_info.dependency_render_graph_nodes = {m_downsample_half_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {half_ping};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {half_pong};

        blur_half_horizontal_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    blur_half_horizontal_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
    blur_half_horizontal_pass_setup_info.execute_command = make_compute_dispatch(half_width, half_height);
    m_blur_half_horizontal_pass_node = graph.CreateRenderGraphNode(resource_operator, blur_half_horizontal_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo blur_half_vertical_pass_setup_info{};
    blur_half_vertical_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    blur_half_vertical_pass_setup_info.debug_group = "Frosted Glass";
    blur_half_vertical_pass_setup_info.debug_name = "Blur Half Vertical";
    blur_half_vertical_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "BlurVerticalMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    blur_half_vertical_pass_setup_info.dependency_render_graph_nodes = {m_blur_half_horizontal_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {half_pong};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_half_blur_final_output};

        blur_half_vertical_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    blur_half_vertical_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
    blur_half_vertical_pass_setup_info.execute_command = make_compute_dispatch(half_width, half_height);
    m_blur_half_vertical_pass_node = graph.CreateRenderGraphNode(resource_operator, blur_half_vertical_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo downsample_quarter_pass_setup_info{};
    downsample_quarter_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    downsample_quarter_pass_setup_info.debug_group = "Frosted Glass";
    downsample_quarter_pass_setup_info.debug_name = "Downsample Quarter";
    downsample_quarter_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "DownsampleMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    downsample_quarter_pass_setup_info.dependency_render_graph_nodes = {m_blur_half_vertical_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_half_blur_final_output};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {quarter_ping};

        downsample_quarter_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    downsample_quarter_pass_setup_info.execute_command = make_compute_dispatch(quarter_width, quarter_height);
    m_downsample_quarter_pass_node = graph.CreateRenderGraphNode(resource_operator, downsample_quarter_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo blur_quarter_horizontal_pass_setup_info{};
    blur_quarter_horizontal_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    blur_quarter_horizontal_pass_setup_info.debug_group = "Frosted Glass";
    blur_quarter_horizontal_pass_setup_info.debug_name = "Blur Quarter Horizontal";
    blur_quarter_horizontal_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "BlurHorizontalMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    blur_quarter_horizontal_pass_setup_info.dependency_render_graph_nodes = {m_downsample_quarter_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {quarter_ping};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {quarter_pong};

        blur_quarter_horizontal_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    blur_quarter_horizontal_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
    blur_quarter_horizontal_pass_setup_info.execute_command = make_compute_dispatch(quarter_width, quarter_height);
    m_blur_quarter_horizontal_pass_node = graph.CreateRenderGraphNode(resource_operator, blur_quarter_horizontal_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo blur_quarter_vertical_pass_setup_info{};
    blur_quarter_vertical_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    blur_quarter_vertical_pass_setup_info.debug_group = "Frosted Glass";
    blur_quarter_vertical_pass_setup_info.debug_name = "Blur Quarter Vertical";
    blur_quarter_vertical_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "BlurVerticalMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    blur_quarter_vertical_pass_setup_info.dependency_render_graph_nodes = {m_blur_quarter_horizontal_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {quarter_pong};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_quarter_blur_final_output};

        blur_quarter_vertical_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    blur_quarter_vertical_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
    blur_quarter_vertical_pass_setup_info.execute_command = make_compute_dispatch(quarter_width, quarter_height);
    m_blur_quarter_vertical_pass_node = graph.CreateRenderGraphNode(resource_operator, blur_quarter_vertical_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo downsample_eighth_pass_setup_info{};
    downsample_eighth_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    downsample_eighth_pass_setup_info.debug_group = "Frosted Glass";
    downsample_eighth_pass_setup_info.debug_name = "Downsample Eighth";
    downsample_eighth_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "DownsampleMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    downsample_eighth_pass_setup_info.dependency_render_graph_nodes = {m_blur_quarter_vertical_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_quarter_blur_final_output};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_eighth_blur_ping};

        downsample_eighth_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    downsample_eighth_pass_setup_info.execute_command = make_compute_dispatch(eighth_width, eighth_height);
    m_downsample_eighth_pass_node = graph.CreateRenderGraphNode(resource_operator, downsample_eighth_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo blur_eighth_horizontal_pass_setup_info{};
    blur_eighth_horizontal_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    blur_eighth_horizontal_pass_setup_info.debug_group = "Frosted Glass";
    blur_eighth_horizontal_pass_setup_info.debug_name = "Blur Eighth Horizontal";
    blur_eighth_horizontal_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "BlurHorizontalMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    blur_eighth_horizontal_pass_setup_info.dependency_render_graph_nodes = {m_downsample_eighth_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_eighth_blur_ping};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_eighth_blur_pong};

        blur_eighth_horizontal_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    blur_eighth_horizontal_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
    blur_eighth_horizontal_pass_setup_info.execute_command = make_compute_dispatch(eighth_width, eighth_height);
    m_blur_eighth_horizontal_pass_node = graph.CreateRenderGraphNode(resource_operator, blur_eighth_horizontal_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo blur_eighth_vertical_pass_setup_info{};
    blur_eighth_vertical_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    blur_eighth_vertical_pass_setup_info.debug_group = "Frosted Glass";
    blur_eighth_vertical_pass_setup_info.debug_name = "Blur Eighth Vertical";
    blur_eighth_vertical_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "BlurVerticalMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    blur_eighth_vertical_pass_setup_info.dependency_render_graph_nodes = {m_blur_eighth_horizontal_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_eighth_blur_pong};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_eighth_blur_final_output};

        blur_eighth_vertical_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    blur_eighth_vertical_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
    blur_eighth_vertical_pass_setup_info.execute_command = make_compute_dispatch(eighth_width, eighth_height);
    m_blur_eighth_vertical_pass_node = graph.CreateRenderGraphNode(resource_operator, blur_eighth_vertical_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo downsample_sixteenth_pass_setup_info{};
    downsample_sixteenth_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    downsample_sixteenth_pass_setup_info.debug_group = "Frosted Glass";
    downsample_sixteenth_pass_setup_info.debug_name = "Downsample Sixteenth";
    downsample_sixteenth_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "DownsampleMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    downsample_sixteenth_pass_setup_info.dependency_render_graph_nodes = {m_blur_eighth_vertical_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_eighth_blur_final_output};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_sixteenth_blur_ping};

        downsample_sixteenth_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    downsample_sixteenth_pass_setup_info.execute_command = make_compute_dispatch(sixteenth_width, sixteenth_height);
    m_downsample_sixteenth_pass_node = graph.CreateRenderGraphNode(resource_operator, downsample_sixteenth_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo blur_sixteenth_horizontal_pass_setup_info{};
    blur_sixteenth_horizontal_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    blur_sixteenth_horizontal_pass_setup_info.debug_group = "Frosted Glass";
    blur_sixteenth_horizontal_pass_setup_info.debug_name = "Blur Sixteenth Horizontal";
    blur_sixteenth_horizontal_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "BlurHorizontalMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    blur_sixteenth_horizontal_pass_setup_info.dependency_render_graph_nodes = {m_downsample_sixteenth_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_sixteenth_blur_ping};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_sixteenth_blur_pong};

        blur_sixteenth_horizontal_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    blur_sixteenth_horizontal_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
    blur_sixteenth_horizontal_pass_setup_info.execute_command = make_compute_dispatch(sixteenth_width, sixteenth_height);
    m_blur_sixteenth_horizontal_pass_node = graph.CreateRenderGraphNode(resource_operator, blur_sixteenth_horizontal_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo blur_sixteenth_vertical_pass_setup_info{};
    blur_sixteenth_vertical_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    blur_sixteenth_vertical_pass_setup_info.debug_group = "Frosted Glass";
    blur_sixteenth_vertical_pass_setup_info.debug_name = "Blur Sixteenth Vertical";
    blur_sixteenth_vertical_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "BlurVerticalMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    blur_sixteenth_vertical_pass_setup_info.dependency_render_graph_nodes = {m_blur_sixteenth_horizontal_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_sixteenth_blur_pong};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_sixteenth_blur_final_output};

        blur_sixteenth_vertical_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    blur_sixteenth_vertical_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
    blur_sixteenth_vertical_pass_setup_info.execute_command = make_compute_dispatch(sixteenth_width, sixteenth_height);
    m_blur_sixteenth_vertical_pass_node = graph.CreateRenderGraphNode(resource_operator, blur_sixteenth_vertical_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo downsample_thirtysecond_pass_setup_info{};
    downsample_thirtysecond_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    downsample_thirtysecond_pass_setup_info.debug_group = "Frosted Glass";
    downsample_thirtysecond_pass_setup_info.debug_name = "Downsample ThirtySecond";
    downsample_thirtysecond_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "DownsampleMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    downsample_thirtysecond_pass_setup_info.dependency_render_graph_nodes = {m_blur_sixteenth_vertical_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_sixteenth_blur_final_output};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_thirtysecond_blur_ping};

        downsample_thirtysecond_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    downsample_thirtysecond_pass_setup_info.execute_command = make_compute_dispatch(thirtysecond_width, thirtysecond_height);
    m_downsample_thirtysecond_pass_node = graph.CreateRenderGraphNode(resource_operator, downsample_thirtysecond_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo blur_thirtysecond_horizontal_pass_setup_info{};
    blur_thirtysecond_horizontal_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    blur_thirtysecond_horizontal_pass_setup_info.debug_group = "Frosted Glass";
    blur_thirtysecond_horizontal_pass_setup_info.debug_name = "Blur ThirtySecond Horizontal";
    blur_thirtysecond_horizontal_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "BlurHorizontalMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    blur_thirtysecond_horizontal_pass_setup_info.dependency_render_graph_nodes = {m_downsample_thirtysecond_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_thirtysecond_blur_ping};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_thirtysecond_blur_pong};

        blur_thirtysecond_horizontal_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    blur_thirtysecond_horizontal_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
    blur_thirtysecond_horizontal_pass_setup_info.execute_command = make_compute_dispatch(thirtysecond_width, thirtysecond_height);
    m_blur_thirtysecond_horizontal_pass_node = graph.CreateRenderGraphNode(resource_operator, blur_thirtysecond_horizontal_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo blur_thirtysecond_vertical_pass_setup_info{};
    blur_thirtysecond_vertical_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    blur_thirtysecond_vertical_pass_setup_info.debug_group = "Frosted Glass";
    blur_thirtysecond_vertical_pass_setup_info.debug_name = "Blur ThirtySecond Vertical";
    blur_thirtysecond_vertical_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "BlurVerticalMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    blur_thirtysecond_vertical_pass_setup_info.dependency_render_graph_nodes = {m_blur_thirtysecond_horizontal_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_thirtysecond_blur_pong};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_thirtysecond_blur_final_output};

        blur_thirtysecond_vertical_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    blur_thirtysecond_vertical_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
    blur_thirtysecond_vertical_pass_setup_info.execute_command = make_compute_dispatch(thirtysecond_width, thirtysecond_height);
    m_blur_thirtysecond_vertical_pass_node = graph.CreateRenderGraphNode(resource_operator, blur_thirtysecond_vertical_pass_setup_info);

    RendererInterface::BufferBindingDesc panel_data_binding_desc{};
    panel_data_binding_desc.binding_type = RendererInterface::BufferBindingDesc::SRV;
    panel_data_binding_desc.buffer_handle = m_frosted_panel_data_handle;
    panel_data_binding_desc.stride = sizeof(FrostedGlassPanelGpuData);
    panel_data_binding_desc.is_structured_buffer = true;
    panel_data_binding_desc.count = MAX_PANEL_COUNT;

    RendererInterface::RenderGraph::RenderPassSetupInfo frosted_mask_parameter_pass_setup_info{};
    frosted_mask_parameter_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    frosted_mask_parameter_pass_setup_info.debug_group = "Frosted Glass";
    frosted_mask_parameter_pass_setup_info.debug_name = "Frosted Mask/Parameter";
    frosted_mask_parameter_pass_setup_info.modules = {m_scene->GetCameraModule()};
    frosted_mask_parameter_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "MaskParameterMain", "Resources/Shaders/FrostedGlass.hlsl"}
    };
    {
        RendererInterface::RenderTargetTextureBindingDesc input_depth_binding_desc{};
        input_depth_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_depth_binding_desc.name = "InputDepthTex";
        input_depth_binding_desc.render_target_texture = {scene_output.GetRenderTargetHandle(*m_scene, "m_base_pass_depth")};

        RendererInterface::RenderTargetTextureBindingDesc input_normal_binding_desc{};
        input_normal_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_normal_binding_desc.name = "InputNormalTex";
        input_normal_binding_desc.render_target_texture = {scene_output.GetRenderTargetHandle(*m_scene, "m_base_pass_normal")};

        RendererInterface::RenderTargetTextureBindingDesc output_mask_param_binding_desc{};
        output_mask_param_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_mask_param_binding_desc.name = "MaskParamOutput";
        output_mask_param_binding_desc.render_target_texture = {m_frosted_mask_parameter_output};

        RendererInterface::RenderTargetTextureBindingDesc output_mask_param_secondary_binding_desc{};
        output_mask_param_secondary_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_mask_param_secondary_binding_desc.name = "MaskParamSecondaryOutput";
        output_mask_param_secondary_binding_desc.render_target_texture = {m_frosted_mask_parameter_secondary_output};

        RendererInterface::RenderTargetTextureBindingDesc output_panel_optics_binding_desc{};
        output_panel_optics_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_panel_optics_binding_desc.name = "PanelOpticsOutput";
        output_panel_optics_binding_desc.render_target_texture = {m_frosted_panel_optics_output};

        RendererInterface::RenderTargetTextureBindingDesc output_panel_optics_secondary_binding_desc{};
        output_panel_optics_secondary_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_panel_optics_secondary_binding_desc.name = "PanelOpticsSecondaryOutput";
        output_panel_optics_secondary_binding_desc.render_target_texture = {m_frosted_panel_optics_secondary_output};

        RendererInterface::RenderTargetTextureBindingDesc output_panel_profile_binding_desc{};
        output_panel_profile_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_panel_profile_binding_desc.name = "PanelProfileOutput";
        output_panel_profile_binding_desc.render_target_texture = {m_frosted_panel_profile_output};

        RendererInterface::RenderTargetTextureBindingDesc output_panel_profile_secondary_binding_desc{};
        output_panel_profile_secondary_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_panel_profile_secondary_binding_desc.name = "PanelProfileSecondaryOutput";
        output_panel_profile_secondary_binding_desc.render_target_texture = {m_frosted_panel_profile_secondary_output};

        frosted_mask_parameter_pass_setup_info.sampled_render_targets = {
            input_depth_binding_desc,
            input_normal_binding_desc,
            output_mask_param_binding_desc,
            output_mask_param_secondary_binding_desc,
            output_panel_optics_binding_desc,
            output_panel_optics_secondary_binding_desc,
            output_panel_profile_binding_desc,
            output_panel_profile_secondary_binding_desc
        };
    }
    frosted_mask_parameter_pass_setup_info.buffer_resources["g_frosted_panels"] = panel_data_binding_desc;
    frosted_mask_parameter_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
    frosted_mask_parameter_pass_setup_info.execute_command = make_compute_dispatch(width, height);
    m_frosted_mask_parameter_pass_node = graph.CreateRenderGraphNode(resource_operator, frosted_mask_parameter_pass_setup_info);

    RendererInterface::RenderExecuteCommand panel_payload_draw_command{};
    panel_payload_draw_command.type = RendererInterface::ExecuteCommandType::DRAW_VERTEX_INSTANCING_COMMAND;
    panel_payload_draw_command.parameter.draw_vertex_instance_command_parameter.vertex_count = 6;
    panel_payload_draw_command.parameter.draw_vertex_instance_command_parameter.instance_count = MAX_PANEL_COUNT;
    panel_payload_draw_command.parameter.draw_vertex_instance_command_parameter.start_vertex_location = 0;
    panel_payload_draw_command.parameter.draw_vertex_instance_command_parameter.start_instance_location = 0;

    RendererInterface::RenderGraph::RenderPassSetupInfo frosted_mask_parameter_raster_front_pass_setup_info{};
    frosted_mask_parameter_raster_front_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::GRAPHICS;
    frosted_mask_parameter_raster_front_pass_setup_info.debug_group = "Frosted Glass";
    frosted_mask_parameter_raster_front_pass_setup_info.debug_name = "Frosted Mask/Parameter Raster Front";
    frosted_mask_parameter_raster_front_pass_setup_info.modules = {m_scene->GetCameraModule()};
    frosted_mask_parameter_raster_front_pass_setup_info.shader_setup_infos = {
        {RendererInterface::VERTEX_SHADER, "PanelPayloadVS", "Resources/Shaders/FrostedGlass.hlsl"},
        {RendererInterface::FRAGMENT_SHADER, "PanelPayloadFrontPS", "Resources/Shaders/FrostedGlass.hlsl"}
    };
    frosted_mask_parameter_raster_front_pass_setup_info.render_state.depth_stencil_mode = RendererInterface::DepthStencilMode::DEPTH_WRITE;
    {
        RendererInterface::RenderTargetBindingDesc color_binding_desc{};
        color_binding_desc.format = RendererInterface::RGBA16_FLOAT;
        color_binding_desc.usage = RendererInterface::RenderPassResourceUsage::COLOR;
        color_binding_desc.need_clear = true;

        RendererInterface::RenderTargetBindingDesc depth_binding_desc{};
        depth_binding_desc.format = RendererInterface::D32;
        depth_binding_desc.usage = RendererInterface::RenderPassResourceUsage::DEPTH_STENCIL;
        depth_binding_desc.need_clear = true;

        frosted_mask_parameter_raster_front_pass_setup_info.render_targets = {
            {m_frosted_mask_parameter_output, color_binding_desc},
            {m_frosted_panel_optics_output, color_binding_desc},
            {m_frosted_panel_profile_output, color_binding_desc},
            {m_frosted_panel_payload_depth, depth_binding_desc}
        };

        RendererInterface::RenderTargetTextureBindingDesc input_depth_binding_desc{};
        input_depth_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_depth_binding_desc.name = "InputDepthTex";
        input_depth_binding_desc.render_target_texture = {scene_output.GetRenderTargetHandle(*m_scene, "m_base_pass_depth")};

        RendererInterface::RenderTargetTextureBindingDesc input_normal_binding_desc{};
        input_normal_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_normal_binding_desc.name = "InputNormalTex";
        input_normal_binding_desc.render_target_texture = {scene_output.GetRenderTargetHandle(*m_scene, "m_base_pass_normal")};

        frosted_mask_parameter_raster_front_pass_setup_info.sampled_render_targets = {
            input_depth_binding_desc,
            input_normal_binding_desc
        };
    }
    frosted_mask_parameter_raster_front_pass_setup_info.buffer_resources["g_frosted_panels"] = panel_data_binding_desc;
    frosted_mask_parameter_raster_front_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
    frosted_mask_parameter_raster_front_pass_setup_info.execute_command = panel_payload_draw_command;
    m_frosted_mask_parameter_raster_front_pass_node =
        graph.CreateRenderGraphNode(resource_operator, frosted_mask_parameter_raster_front_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo frosted_mask_parameter_raster_back_pass_setup_info{};
    frosted_mask_parameter_raster_back_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::GRAPHICS;
    frosted_mask_parameter_raster_back_pass_setup_info.debug_group = "Frosted Glass";
    frosted_mask_parameter_raster_back_pass_setup_info.debug_name = "Frosted Mask/Parameter Raster Back";
    frosted_mask_parameter_raster_back_pass_setup_info.modules = {m_scene->GetCameraModule()};
    frosted_mask_parameter_raster_back_pass_setup_info.shader_setup_infos = {
        {RendererInterface::VERTEX_SHADER, "PanelPayloadVS", "Resources/Shaders/FrostedGlass.hlsl"},
        {RendererInterface::FRAGMENT_SHADER, "PanelPayloadBackPS", "Resources/Shaders/FrostedGlass.hlsl"}
    };
    frosted_mask_parameter_raster_back_pass_setup_info.dependency_render_graph_nodes = {m_frosted_mask_parameter_raster_front_pass_node};
    frosted_mask_parameter_raster_back_pass_setup_info.render_state.depth_stencil_mode = RendererInterface::DepthStencilMode::DEPTH_WRITE;
    {
        RendererInterface::RenderTargetBindingDesc color_binding_desc{};
        color_binding_desc.format = RendererInterface::RGBA16_FLOAT;
        color_binding_desc.usage = RendererInterface::RenderPassResourceUsage::COLOR;
        color_binding_desc.need_clear = true;

        RendererInterface::RenderTargetBindingDesc depth_binding_desc{};
        depth_binding_desc.format = RendererInterface::D32;
        depth_binding_desc.usage = RendererInterface::RenderPassResourceUsage::DEPTH_STENCIL;
        depth_binding_desc.need_clear = true;

        frosted_mask_parameter_raster_back_pass_setup_info.render_targets = {
            {m_frosted_mask_parameter_secondary_output, color_binding_desc},
            {m_frosted_panel_optics_secondary_output, color_binding_desc},
            {m_frosted_panel_profile_secondary_output, color_binding_desc},
            {m_frosted_panel_payload_depth_secondary, depth_binding_desc}
        };

        RendererInterface::RenderTargetTextureBindingDesc input_depth_binding_desc{};
        input_depth_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_depth_binding_desc.name = "InputDepthTex";
        input_depth_binding_desc.render_target_texture = {scene_output.GetRenderTargetHandle(*m_scene, "m_base_pass_depth")};

        RendererInterface::RenderTargetTextureBindingDesc input_normal_binding_desc{};
        input_normal_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_normal_binding_desc.name = "InputNormalTex";
        input_normal_binding_desc.render_target_texture = {scene_output.GetRenderTargetHandle(*m_scene, "m_base_pass_normal")};

        RendererInterface::RenderTargetTextureBindingDesc front_mask_binding_desc{};
        front_mask_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        front_mask_binding_desc.name = "FrontMaskParamTex";
        front_mask_binding_desc.render_target_texture = {m_frosted_mask_parameter_output};

        frosted_mask_parameter_raster_back_pass_setup_info.sampled_render_targets = {
            input_depth_binding_desc,
            input_normal_binding_desc,
            front_mask_binding_desc
        };
    }
    frosted_mask_parameter_raster_back_pass_setup_info.buffer_resources["g_frosted_panels"] = panel_data_binding_desc;
    frosted_mask_parameter_raster_back_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
    frosted_mask_parameter_raster_back_pass_setup_info.execute_command = panel_payload_draw_command;
    m_frosted_mask_parameter_raster_back_pass_node =
        graph.CreateRenderGraphNode(resource_operator, frosted_mask_parameter_raster_back_pass_setup_info);
    m_panel_payload_raster_ready =
        m_frosted_mask_parameter_raster_front_pass_node != NULL_HANDLE &&
        m_frosted_mask_parameter_raster_back_pass_node != NULL_HANDLE;

    const RendererInterface::RenderTargetHandle velocity_rt =
        scene_output.GetRenderTargetHandle(*m_scene, "m_base_pass_velocity");

    RendererInterface::RenderGraph::RenderPassSetupInfo frosted_composite_back_pass_setup_info{};
    frosted_composite_back_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    frosted_composite_back_pass_setup_info.debug_group = "Frosted Glass";
    frosted_composite_back_pass_setup_info.debug_name = "Frosted Composite Back";
    frosted_composite_back_pass_setup_info.modules = {m_scene->GetCameraModule()};
    frosted_composite_back_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "CompositeBackMain", "Resources/Shaders/FrostedGlass.hlsl"}
    };
    frosted_composite_back_pass_setup_info.dependency_render_graph_nodes = {m_blur_thirtysecond_vertical_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_color_binding_desc{};
        input_color_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_color_binding_desc.name = "InputColorTex";
        input_color_binding_desc.render_target_texture = {m_lighting->GetLightingOutput()};

        RendererInterface::RenderTargetTextureBindingDesc input_blurred_binding_desc{};
        input_blurred_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_blurred_binding_desc.name = "BlurredColorTex";
        input_blurred_binding_desc.render_target_texture = {m_half_blur_final_output};

        RendererInterface::RenderTargetTextureBindingDesc input_quarter_blurred_binding_desc{};
        input_quarter_blurred_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_quarter_blurred_binding_desc.name = "QuarterBlurredColorTex";
        input_quarter_blurred_binding_desc.render_target_texture = {m_quarter_blur_final_output};

        RendererInterface::RenderTargetTextureBindingDesc input_eighth_blurred_binding_desc{};
        input_eighth_blurred_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_eighth_blurred_binding_desc.name = "EighthBlurredColorTex";
        input_eighth_blurred_binding_desc.render_target_texture = {m_eighth_blur_final_output};

        RendererInterface::RenderTargetTextureBindingDesc input_sixteenth_blurred_binding_desc{};
        input_sixteenth_blurred_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_sixteenth_blurred_binding_desc.name = "SixteenthBlurredColorTex";
        input_sixteenth_blurred_binding_desc.render_target_texture = {m_sixteenth_blur_final_output};

        RendererInterface::RenderTargetTextureBindingDesc input_thirtysecond_blurred_binding_desc{};
        input_thirtysecond_blurred_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_thirtysecond_blurred_binding_desc.name = "ThirtySecondBlurredColorTex";
        input_thirtysecond_blurred_binding_desc.render_target_texture = {m_thirtysecond_blur_final_output};

        RendererInterface::RenderTargetTextureBindingDesc input_mask_param_binding_desc{};
        input_mask_param_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_mask_param_binding_desc.name = "MaskParamTex";
        input_mask_param_binding_desc.render_target_texture = {m_frosted_mask_parameter_output};

        RendererInterface::RenderTargetTextureBindingDesc input_mask_param_secondary_binding_desc{};
        input_mask_param_secondary_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_mask_param_secondary_binding_desc.name = "MaskParamSecondaryTex";
        input_mask_param_secondary_binding_desc.render_target_texture = {m_frosted_mask_parameter_secondary_output};

        RendererInterface::RenderTargetTextureBindingDesc input_panel_optics_binding_desc{};
        input_panel_optics_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_panel_optics_binding_desc.name = "PanelOpticsTex";
        input_panel_optics_binding_desc.render_target_texture = {m_frosted_panel_optics_output};

        RendererInterface::RenderTargetTextureBindingDesc input_panel_optics_secondary_binding_desc{};
        input_panel_optics_secondary_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_panel_optics_secondary_binding_desc.name = "PanelOpticsSecondaryTex";
        input_panel_optics_secondary_binding_desc.render_target_texture = {m_frosted_panel_optics_secondary_output};

        RendererInterface::RenderTargetTextureBindingDesc input_panel_profile_binding_desc{};
        input_panel_profile_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_panel_profile_binding_desc.name = "PanelProfileTex";
        input_panel_profile_binding_desc.render_target_texture = {m_frosted_panel_profile_output};

        RendererInterface::RenderTargetTextureBindingDesc input_panel_profile_secondary_binding_desc{};
        input_panel_profile_secondary_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_panel_profile_secondary_binding_desc.name = "PanelProfileSecondaryTex";
        input_panel_profile_secondary_binding_desc.render_target_texture = {m_frosted_panel_profile_secondary_output};

        RendererInterface::RenderTargetTextureBindingDesc output_back_composite_binding_desc{};
        output_back_composite_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_back_composite_binding_desc.name = "BackCompositeOutput";
        output_back_composite_binding_desc.render_target_texture = {m_frosted_back_composite_output};

        frosted_composite_back_pass_setup_info.sampled_render_targets = {
            input_color_binding_desc,
            input_blurred_binding_desc,
            input_quarter_blurred_binding_desc,
            input_eighth_blurred_binding_desc,
            input_sixteenth_blurred_binding_desc,
            input_thirtysecond_blurred_binding_desc,
            input_mask_param_binding_desc,
            input_mask_param_secondary_binding_desc,
            input_panel_optics_binding_desc,
            input_panel_optics_secondary_binding_desc,
            input_panel_profile_binding_desc,
            input_panel_profile_secondary_binding_desc,
            output_back_composite_binding_desc
        };
    }
    frosted_composite_back_pass_setup_info.buffer_resources["g_frosted_panels"] = panel_data_binding_desc;
    frosted_composite_back_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
    frosted_composite_back_pass_setup_info.execute_command = make_compute_dispatch(width, height);
    m_frosted_composite_back_pass_node = graph.CreateRenderGraphNode(resource_operator, frosted_composite_back_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo downsample_half_multilayer_pass_setup_info{};
    downsample_half_multilayer_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    downsample_half_multilayer_pass_setup_info.debug_group = "Frosted Glass";
    downsample_half_multilayer_pass_setup_info.debug_name = "Downsample Half Multilayer";
    downsample_half_multilayer_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "DownsampleMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    downsample_half_multilayer_pass_setup_info.dependency_render_graph_nodes = {m_frosted_composite_back_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_frosted_back_composite_output};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_half_multilayer_ping};

        downsample_half_multilayer_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    downsample_half_multilayer_pass_setup_info.execute_command = make_compute_dispatch(half_width, half_height);
    m_downsample_half_multilayer_pass_node = graph.CreateRenderGraphNode(resource_operator, downsample_half_multilayer_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo blur_half_multilayer_horizontal_pass_setup_info{};
    blur_half_multilayer_horizontal_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    blur_half_multilayer_horizontal_pass_setup_info.debug_group = "Frosted Glass";
    blur_half_multilayer_horizontal_pass_setup_info.debug_name = "Blur Half Multilayer Horizontal";
    blur_half_multilayer_horizontal_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "BlurHorizontalMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    blur_half_multilayer_horizontal_pass_setup_info.dependency_render_graph_nodes = {m_downsample_half_multilayer_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_half_multilayer_ping};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_half_multilayer_pong};

        blur_half_multilayer_horizontal_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    blur_half_multilayer_horizontal_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
    blur_half_multilayer_horizontal_pass_setup_info.execute_command = make_compute_dispatch(half_width, half_height);
    m_blur_half_multilayer_horizontal_pass_node = graph.CreateRenderGraphNode(resource_operator, blur_half_multilayer_horizontal_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo blur_half_multilayer_vertical_pass_setup_info{};
    blur_half_multilayer_vertical_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    blur_half_multilayer_vertical_pass_setup_info.debug_group = "Frosted Glass";
    blur_half_multilayer_vertical_pass_setup_info.debug_name = "Blur Half Multilayer Vertical";
    blur_half_multilayer_vertical_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "BlurVerticalMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    blur_half_multilayer_vertical_pass_setup_info.dependency_render_graph_nodes = {m_blur_half_multilayer_horizontal_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_half_multilayer_pong};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_half_multilayer_blur_final_output};

        blur_half_multilayer_vertical_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    blur_half_multilayer_vertical_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
    blur_half_multilayer_vertical_pass_setup_info.execute_command = make_compute_dispatch(half_width, half_height);
    m_blur_half_multilayer_vertical_pass_node = graph.CreateRenderGraphNode(resource_operator, blur_half_multilayer_vertical_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo downsample_quarter_multilayer_pass_setup_info{};
    downsample_quarter_multilayer_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    downsample_quarter_multilayer_pass_setup_info.debug_group = "Frosted Glass";
    downsample_quarter_multilayer_pass_setup_info.debug_name = "Downsample Quarter Multilayer";
    downsample_quarter_multilayer_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "DownsampleMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    downsample_quarter_multilayer_pass_setup_info.dependency_render_graph_nodes = {m_blur_half_multilayer_vertical_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_half_multilayer_blur_final_output};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_quarter_multilayer_ping};

        downsample_quarter_multilayer_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    downsample_quarter_multilayer_pass_setup_info.execute_command = make_compute_dispatch(quarter_width, quarter_height);
    m_downsample_quarter_multilayer_pass_node = graph.CreateRenderGraphNode(resource_operator, downsample_quarter_multilayer_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo blur_quarter_multilayer_horizontal_pass_setup_info{};
    blur_quarter_multilayer_horizontal_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    blur_quarter_multilayer_horizontal_pass_setup_info.debug_group = "Frosted Glass";
    blur_quarter_multilayer_horizontal_pass_setup_info.debug_name = "Blur Quarter Multilayer Horizontal";
    blur_quarter_multilayer_horizontal_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "BlurHorizontalMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    blur_quarter_multilayer_horizontal_pass_setup_info.dependency_render_graph_nodes = {m_downsample_quarter_multilayer_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_quarter_multilayer_ping};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_quarter_multilayer_pong};

        blur_quarter_multilayer_horizontal_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    blur_quarter_multilayer_horizontal_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
    blur_quarter_multilayer_horizontal_pass_setup_info.execute_command = make_compute_dispatch(quarter_width, quarter_height);
    m_blur_quarter_multilayer_horizontal_pass_node = graph.CreateRenderGraphNode(resource_operator, blur_quarter_multilayer_horizontal_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo blur_quarter_multilayer_vertical_pass_setup_info{};
    blur_quarter_multilayer_vertical_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    blur_quarter_multilayer_vertical_pass_setup_info.debug_group = "Frosted Glass";
    blur_quarter_multilayer_vertical_pass_setup_info.debug_name = "Blur Quarter Multilayer Vertical";
    blur_quarter_multilayer_vertical_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "BlurVerticalMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    blur_quarter_multilayer_vertical_pass_setup_info.dependency_render_graph_nodes = {m_blur_quarter_multilayer_horizontal_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_quarter_multilayer_pong};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_quarter_multilayer_blur_final_output};

        blur_quarter_multilayer_vertical_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    blur_quarter_multilayer_vertical_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
    blur_quarter_multilayer_vertical_pass_setup_info.execute_command = make_compute_dispatch(quarter_width, quarter_height);
    m_blur_quarter_multilayer_vertical_pass_node = graph.CreateRenderGraphNode(resource_operator, blur_quarter_multilayer_vertical_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo downsample_eighth_multilayer_pass_setup_info{};
    downsample_eighth_multilayer_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    downsample_eighth_multilayer_pass_setup_info.debug_group = "Frosted Glass";
    downsample_eighth_multilayer_pass_setup_info.debug_name = "Downsample Eighth Multilayer";
    downsample_eighth_multilayer_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "DownsampleMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    downsample_eighth_multilayer_pass_setup_info.dependency_render_graph_nodes = {m_blur_quarter_multilayer_vertical_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_quarter_multilayer_blur_final_output};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_eighth_multilayer_ping};

        downsample_eighth_multilayer_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    downsample_eighth_multilayer_pass_setup_info.execute_command = make_compute_dispatch(eighth_width, eighth_height);
    m_downsample_eighth_multilayer_pass_node = graph.CreateRenderGraphNode(resource_operator, downsample_eighth_multilayer_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo blur_eighth_multilayer_horizontal_pass_setup_info{};
    blur_eighth_multilayer_horizontal_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    blur_eighth_multilayer_horizontal_pass_setup_info.debug_group = "Frosted Glass";
    blur_eighth_multilayer_horizontal_pass_setup_info.debug_name = "Blur Eighth Multilayer Horizontal";
    blur_eighth_multilayer_horizontal_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "BlurHorizontalMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    blur_eighth_multilayer_horizontal_pass_setup_info.dependency_render_graph_nodes = {m_downsample_eighth_multilayer_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_eighth_multilayer_ping};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_eighth_multilayer_pong};

        blur_eighth_multilayer_horizontal_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    blur_eighth_multilayer_horizontal_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
    blur_eighth_multilayer_horizontal_pass_setup_info.execute_command = make_compute_dispatch(eighth_width, eighth_height);
    m_blur_eighth_multilayer_horizontal_pass_node = graph.CreateRenderGraphNode(resource_operator, blur_eighth_multilayer_horizontal_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo blur_eighth_multilayer_vertical_pass_setup_info{};
    blur_eighth_multilayer_vertical_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    blur_eighth_multilayer_vertical_pass_setup_info.debug_group = "Frosted Glass";
    blur_eighth_multilayer_vertical_pass_setup_info.debug_name = "Blur Eighth Multilayer Vertical";
    blur_eighth_multilayer_vertical_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "BlurVerticalMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    blur_eighth_multilayer_vertical_pass_setup_info.dependency_render_graph_nodes = {m_blur_eighth_multilayer_horizontal_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_eighth_multilayer_pong};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_eighth_multilayer_blur_final_output};

        blur_eighth_multilayer_vertical_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    blur_eighth_multilayer_vertical_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
    blur_eighth_multilayer_vertical_pass_setup_info.execute_command = make_compute_dispatch(eighth_width, eighth_height);
    m_blur_eighth_multilayer_vertical_pass_node = graph.CreateRenderGraphNode(resource_operator, blur_eighth_multilayer_vertical_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo downsample_sixteenth_multilayer_pass_setup_info{};
    downsample_sixteenth_multilayer_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    downsample_sixteenth_multilayer_pass_setup_info.debug_group = "Frosted Glass";
    downsample_sixteenth_multilayer_pass_setup_info.debug_name = "Downsample Sixteenth Multilayer";
    downsample_sixteenth_multilayer_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "DownsampleMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    downsample_sixteenth_multilayer_pass_setup_info.dependency_render_graph_nodes = {m_blur_eighth_multilayer_vertical_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_eighth_multilayer_blur_final_output};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_sixteenth_multilayer_ping};

        downsample_sixteenth_multilayer_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    downsample_sixteenth_multilayer_pass_setup_info.execute_command = make_compute_dispatch(sixteenth_width, sixteenth_height);
    m_downsample_sixteenth_multilayer_pass_node = graph.CreateRenderGraphNode(resource_operator, downsample_sixteenth_multilayer_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo blur_sixteenth_multilayer_horizontal_pass_setup_info{};
    blur_sixteenth_multilayer_horizontal_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    blur_sixteenth_multilayer_horizontal_pass_setup_info.debug_group = "Frosted Glass";
    blur_sixteenth_multilayer_horizontal_pass_setup_info.debug_name = "Blur Sixteenth Multilayer Horizontal";
    blur_sixteenth_multilayer_horizontal_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "BlurHorizontalMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    blur_sixteenth_multilayer_horizontal_pass_setup_info.dependency_render_graph_nodes = {m_downsample_sixteenth_multilayer_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_sixteenth_multilayer_ping};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_sixteenth_multilayer_pong};

        blur_sixteenth_multilayer_horizontal_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    blur_sixteenth_multilayer_horizontal_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
    blur_sixteenth_multilayer_horizontal_pass_setup_info.execute_command = make_compute_dispatch(sixteenth_width, sixteenth_height);
    m_blur_sixteenth_multilayer_horizontal_pass_node = graph.CreateRenderGraphNode(resource_operator, blur_sixteenth_multilayer_horizontal_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo blur_sixteenth_multilayer_vertical_pass_setup_info{};
    blur_sixteenth_multilayer_vertical_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    blur_sixteenth_multilayer_vertical_pass_setup_info.debug_group = "Frosted Glass";
    blur_sixteenth_multilayer_vertical_pass_setup_info.debug_name = "Blur Sixteenth Multilayer Vertical";
    blur_sixteenth_multilayer_vertical_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "BlurVerticalMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    blur_sixteenth_multilayer_vertical_pass_setup_info.dependency_render_graph_nodes = {m_blur_sixteenth_multilayer_horizontal_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_sixteenth_multilayer_pong};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_sixteenth_multilayer_blur_final_output};

        blur_sixteenth_multilayer_vertical_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    blur_sixteenth_multilayer_vertical_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
    blur_sixteenth_multilayer_vertical_pass_setup_info.execute_command = make_compute_dispatch(sixteenth_width, sixteenth_height);
    m_blur_sixteenth_multilayer_vertical_pass_node = graph.CreateRenderGraphNode(resource_operator, blur_sixteenth_multilayer_vertical_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo downsample_thirtysecond_multilayer_pass_setup_info{};
    downsample_thirtysecond_multilayer_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    downsample_thirtysecond_multilayer_pass_setup_info.debug_group = "Frosted Glass";
    downsample_thirtysecond_multilayer_pass_setup_info.debug_name = "Downsample ThirtySecond Multilayer";
    downsample_thirtysecond_multilayer_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "DownsampleMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    downsample_thirtysecond_multilayer_pass_setup_info.dependency_render_graph_nodes = {m_blur_sixteenth_multilayer_vertical_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_sixteenth_multilayer_blur_final_output};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_thirtysecond_multilayer_ping};

        downsample_thirtysecond_multilayer_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    downsample_thirtysecond_multilayer_pass_setup_info.execute_command = make_compute_dispatch(thirtysecond_width, thirtysecond_height);
    m_downsample_thirtysecond_multilayer_pass_node = graph.CreateRenderGraphNode(resource_operator, downsample_thirtysecond_multilayer_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo blur_thirtysecond_multilayer_horizontal_pass_setup_info{};
    blur_thirtysecond_multilayer_horizontal_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    blur_thirtysecond_multilayer_horizontal_pass_setup_info.debug_group = "Frosted Glass";
    blur_thirtysecond_multilayer_horizontal_pass_setup_info.debug_name = "Blur ThirtySecond Multilayer Horizontal";
    blur_thirtysecond_multilayer_horizontal_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "BlurHorizontalMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    blur_thirtysecond_multilayer_horizontal_pass_setup_info.dependency_render_graph_nodes = {m_downsample_thirtysecond_multilayer_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_thirtysecond_multilayer_ping};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_thirtysecond_multilayer_pong};

        blur_thirtysecond_multilayer_horizontal_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    blur_thirtysecond_multilayer_horizontal_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
    blur_thirtysecond_multilayer_horizontal_pass_setup_info.execute_command = make_compute_dispatch(thirtysecond_width, thirtysecond_height);
    m_blur_thirtysecond_multilayer_horizontal_pass_node = graph.CreateRenderGraphNode(resource_operator, blur_thirtysecond_multilayer_horizontal_pass_setup_info);

    RendererInterface::RenderGraph::RenderPassSetupInfo blur_thirtysecond_multilayer_vertical_pass_setup_info{};
    blur_thirtysecond_multilayer_vertical_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    blur_thirtysecond_multilayer_vertical_pass_setup_info.debug_group = "Frosted Glass";
    blur_thirtysecond_multilayer_vertical_pass_setup_info.debug_name = "Blur ThirtySecond Multilayer Vertical";
    blur_thirtysecond_multilayer_vertical_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "BlurVerticalMain", "Resources/Shaders/FrostedGlassPostfx.hlsl"}
    };
    blur_thirtysecond_multilayer_vertical_pass_setup_info.dependency_render_graph_nodes = {m_blur_thirtysecond_multilayer_horizontal_pass_node};
    {
        RendererInterface::RenderTargetTextureBindingDesc input_binding_desc{};
        input_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        input_binding_desc.name = "InputTex";
        input_binding_desc.render_target_texture = {m_thirtysecond_multilayer_pong};

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "OutputTex";
        output_binding_desc.render_target_texture = {m_thirtysecond_multilayer_blur_final_output};

        blur_thirtysecond_multilayer_vertical_pass_setup_info.sampled_render_targets = {
            input_binding_desc,
            output_binding_desc
        };
    }
    blur_thirtysecond_multilayer_vertical_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
    blur_thirtysecond_multilayer_vertical_pass_setup_info.execute_command = make_compute_dispatch(thirtysecond_width, thirtysecond_height);
    m_blur_thirtysecond_multilayer_vertical_pass_node = graph.CreateRenderGraphNode(resource_operator, blur_thirtysecond_multilayer_vertical_pass_setup_info);

    auto create_frosted_front_composite_node =
        [&](const char* debug_name, RendererInterface::RenderTargetHandle history_read, RendererInterface::RenderTargetHandle history_write)
        -> RendererInterface::RenderGraphNodeHandle
    {
        RendererInterface::RenderGraph::RenderPassSetupInfo frosted_front_composite_pass_setup_info{};
        frosted_front_composite_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
        frosted_front_composite_pass_setup_info.debug_group = "Frosted Glass";
        frosted_front_composite_pass_setup_info.debug_name = debug_name;
        frosted_front_composite_pass_setup_info.modules = {m_scene->GetCameraModule()};
        frosted_front_composite_pass_setup_info.shader_setup_infos = {
            {RendererInterface::COMPUTE_SHADER, "CompositeFrontMain", "Resources/Shaders/FrostedGlass.hlsl"}
        };
        frosted_front_composite_pass_setup_info.dependency_render_graph_nodes = {m_blur_thirtysecond_multilayer_vertical_pass_node};
        {
            RendererInterface::RenderTargetTextureBindingDesc input_back_composite_binding_desc{};
            input_back_composite_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_back_composite_binding_desc.name = "BackCompositeTex";
            input_back_composite_binding_desc.render_target_texture = {m_frosted_back_composite_output};

            RendererInterface::RenderTargetTextureBindingDesc input_blurred_binding_desc{};
            input_blurred_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_blurred_binding_desc.name = "BlurredColorTex";
            input_blurred_binding_desc.render_target_texture = {m_half_multilayer_blur_final_output};

            RendererInterface::RenderTargetTextureBindingDesc input_quarter_blurred_binding_desc{};
            input_quarter_blurred_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_quarter_blurred_binding_desc.name = "QuarterBlurredColorTex";
            input_quarter_blurred_binding_desc.render_target_texture = {m_quarter_multilayer_blur_final_output};

            RendererInterface::RenderTargetTextureBindingDesc input_eighth_blurred_binding_desc{};
            input_eighth_blurred_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_eighth_blurred_binding_desc.name = "EighthBlurredColorTex";
            input_eighth_blurred_binding_desc.render_target_texture = {m_eighth_multilayer_blur_final_output};

            RendererInterface::RenderTargetTextureBindingDesc input_sixteenth_blurred_binding_desc{};
            input_sixteenth_blurred_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_sixteenth_blurred_binding_desc.name = "SixteenthBlurredColorTex";
            input_sixteenth_blurred_binding_desc.render_target_texture = {m_sixteenth_multilayer_blur_final_output};

            RendererInterface::RenderTargetTextureBindingDesc input_thirtysecond_blurred_binding_desc{};
            input_thirtysecond_blurred_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_thirtysecond_blurred_binding_desc.name = "ThirtySecondBlurredColorTex";
            input_thirtysecond_blurred_binding_desc.render_target_texture = {m_thirtysecond_multilayer_blur_final_output};

            RendererInterface::RenderTargetTextureBindingDesc input_mask_param_binding_desc{};
            input_mask_param_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_mask_param_binding_desc.name = "MaskParamTex";
            input_mask_param_binding_desc.render_target_texture = {m_frosted_mask_parameter_output};

            RendererInterface::RenderTargetTextureBindingDesc input_mask_param_secondary_binding_desc{};
            input_mask_param_secondary_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_mask_param_secondary_binding_desc.name = "MaskParamSecondaryTex";
            input_mask_param_secondary_binding_desc.render_target_texture = {m_frosted_mask_parameter_secondary_output};

            RendererInterface::RenderTargetTextureBindingDesc input_panel_optics_binding_desc{};
            input_panel_optics_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_panel_optics_binding_desc.name = "PanelOpticsTex";
            input_panel_optics_binding_desc.render_target_texture = {m_frosted_panel_optics_output};

            RendererInterface::RenderTargetTextureBindingDesc input_panel_optics_secondary_binding_desc{};
            input_panel_optics_secondary_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_panel_optics_secondary_binding_desc.name = "PanelOpticsSecondaryTex";
            input_panel_optics_secondary_binding_desc.render_target_texture = {m_frosted_panel_optics_secondary_output};

            RendererInterface::RenderTargetTextureBindingDesc input_panel_profile_binding_desc{};
            input_panel_profile_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_panel_profile_binding_desc.name = "PanelProfileTex";
            input_panel_profile_binding_desc.render_target_texture = {m_frosted_panel_profile_output};

            RendererInterface::RenderTargetTextureBindingDesc input_panel_profile_secondary_binding_desc{};
            input_panel_profile_secondary_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_panel_profile_secondary_binding_desc.name = "PanelProfileSecondaryTex";
            input_panel_profile_secondary_binding_desc.render_target_texture = {m_frosted_panel_profile_secondary_output};

            RendererInterface::RenderTargetTextureBindingDesc input_history_binding_desc{};
            input_history_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_history_binding_desc.name = "HistoryInputTex";
            input_history_binding_desc.render_target_texture = {history_read};

            RendererInterface::RenderTargetTextureBindingDesc input_velocity_binding_desc{};
            input_velocity_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_velocity_binding_desc.name = "VelocityTex";
            input_velocity_binding_desc.render_target_texture = {velocity_rt};

            RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
            output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
            output_binding_desc.name = "Output";
            output_binding_desc.render_target_texture = {m_frosted_pass_output};

            RendererInterface::RenderTargetTextureBindingDesc history_output_binding_desc{};
            history_output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
            history_output_binding_desc.name = "HistoryOutputTex";
            history_output_binding_desc.render_target_texture = {history_write};

            frosted_front_composite_pass_setup_info.sampled_render_targets = {
                input_back_composite_binding_desc,
                input_blurred_binding_desc,
                input_quarter_blurred_binding_desc,
                input_eighth_blurred_binding_desc,
                input_sixteenth_blurred_binding_desc,
                input_thirtysecond_blurred_binding_desc,
                input_mask_param_binding_desc,
                input_mask_param_secondary_binding_desc,
                input_panel_optics_binding_desc,
                input_panel_optics_secondary_binding_desc,
                input_panel_profile_binding_desc,
                input_panel_profile_secondary_binding_desc,
                input_history_binding_desc,
                input_velocity_binding_desc,
                output_binding_desc,
                history_output_binding_desc
            };
        }
        frosted_front_composite_pass_setup_info.buffer_resources["g_frosted_panels"] = panel_data_binding_desc;
        frosted_front_composite_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
        frosted_front_composite_pass_setup_info.execute_command = make_compute_dispatch(width, height);
        return graph.CreateRenderGraphNode(resource_operator, frosted_front_composite_pass_setup_info);
    };

    m_frosted_composite_front_history_ab_pass_node =
        create_frosted_front_composite_node("Frosted Composite Front History A->B", m_temporal_history_a, m_temporal_history_b);
    m_frosted_composite_front_history_ba_pass_node =
        create_frosted_front_composite_node("Frosted Composite Front History B->A", m_temporal_history_b, m_temporal_history_a);

    auto create_frosted_composite_node =
        [&](const char* debug_name, RendererInterface::RenderTargetHandle history_read, RendererInterface::RenderTargetHandle history_write)
        -> RendererInterface::RenderGraphNodeHandle
    {
        RendererInterface::RenderGraph::RenderPassSetupInfo frosted_composite_pass_setup_info{};
        frosted_composite_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
        frosted_composite_pass_setup_info.debug_group = "Frosted Glass";
        frosted_composite_pass_setup_info.debug_name = debug_name;
        frosted_composite_pass_setup_info.modules = {m_scene->GetCameraModule()};
        frosted_composite_pass_setup_info.shader_setup_infos = {
            {RendererInterface::COMPUTE_SHADER, "CompositeMain", "Resources/Shaders/FrostedGlass.hlsl"}
        };
        frosted_composite_pass_setup_info.dependency_render_graph_nodes = {m_blur_thirtysecond_vertical_pass_node};
        {
            RendererInterface::RenderTargetTextureBindingDesc input_color_binding_desc{};
            input_color_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_color_binding_desc.name = "InputColorTex";
            input_color_binding_desc.render_target_texture = {m_lighting->GetLightingOutput()};

            RendererInterface::RenderTargetTextureBindingDesc input_blurred_binding_desc{};
            input_blurred_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_blurred_binding_desc.name = "BlurredColorTex";
            input_blurred_binding_desc.render_target_texture = {m_half_blur_final_output};

            RendererInterface::RenderTargetTextureBindingDesc input_quarter_blurred_binding_desc{};
            input_quarter_blurred_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_quarter_blurred_binding_desc.name = "QuarterBlurredColorTex";
            input_quarter_blurred_binding_desc.render_target_texture = {m_quarter_blur_final_output};

            RendererInterface::RenderTargetTextureBindingDesc input_eighth_blurred_binding_desc{};
            input_eighth_blurred_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_eighth_blurred_binding_desc.name = "EighthBlurredColorTex";
            input_eighth_blurred_binding_desc.render_target_texture = {m_eighth_blur_final_output};

            RendererInterface::RenderTargetTextureBindingDesc input_sixteenth_blurred_binding_desc{};
            input_sixteenth_blurred_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_sixteenth_blurred_binding_desc.name = "SixteenthBlurredColorTex";
            input_sixteenth_blurred_binding_desc.render_target_texture = {m_sixteenth_blur_final_output};

            RendererInterface::RenderTargetTextureBindingDesc input_thirtysecond_blurred_binding_desc{};
            input_thirtysecond_blurred_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_thirtysecond_blurred_binding_desc.name = "ThirtySecondBlurredColorTex";
            input_thirtysecond_blurred_binding_desc.render_target_texture = {m_thirtysecond_blur_final_output};

            RendererInterface::RenderTargetTextureBindingDesc input_mask_param_binding_desc{};
            input_mask_param_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_mask_param_binding_desc.name = "MaskParamTex";
            input_mask_param_binding_desc.render_target_texture = {m_frosted_mask_parameter_output};

            RendererInterface::RenderTargetTextureBindingDesc input_mask_param_secondary_binding_desc{};
            input_mask_param_secondary_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_mask_param_secondary_binding_desc.name = "MaskParamSecondaryTex";
            input_mask_param_secondary_binding_desc.render_target_texture = {m_frosted_mask_parameter_secondary_output};

            RendererInterface::RenderTargetTextureBindingDesc input_panel_optics_binding_desc{};
            input_panel_optics_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_panel_optics_binding_desc.name = "PanelOpticsTex";
            input_panel_optics_binding_desc.render_target_texture = {m_frosted_panel_optics_output};

            RendererInterface::RenderTargetTextureBindingDesc input_panel_optics_secondary_binding_desc{};
            input_panel_optics_secondary_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_panel_optics_secondary_binding_desc.name = "PanelOpticsSecondaryTex";
            input_panel_optics_secondary_binding_desc.render_target_texture = {m_frosted_panel_optics_secondary_output};

            RendererInterface::RenderTargetTextureBindingDesc input_panel_profile_binding_desc{};
            input_panel_profile_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_panel_profile_binding_desc.name = "PanelProfileTex";
            input_panel_profile_binding_desc.render_target_texture = {m_frosted_panel_profile_output};

            RendererInterface::RenderTargetTextureBindingDesc input_panel_profile_secondary_binding_desc{};
            input_panel_profile_secondary_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_panel_profile_secondary_binding_desc.name = "PanelProfileSecondaryTex";
            input_panel_profile_secondary_binding_desc.render_target_texture = {m_frosted_panel_profile_secondary_output};

            RendererInterface::RenderTargetTextureBindingDesc input_history_binding_desc{};
            input_history_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_history_binding_desc.name = "HistoryInputTex";
            input_history_binding_desc.render_target_texture = {history_read};

            RendererInterface::RenderTargetTextureBindingDesc input_velocity_binding_desc{};
            input_velocity_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
            input_velocity_binding_desc.name = "VelocityTex";
            input_velocity_binding_desc.render_target_texture = {velocity_rt};

            RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
            output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
            output_binding_desc.name = "Output";
            output_binding_desc.render_target_texture = {m_frosted_pass_output};

            RendererInterface::RenderTargetTextureBindingDesc history_output_binding_desc{};
            history_output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
            history_output_binding_desc.name = "HistoryOutputTex";
            history_output_binding_desc.render_target_texture = {history_write};

            frosted_composite_pass_setup_info.sampled_render_targets = {
                input_color_binding_desc,
                input_blurred_binding_desc,
                input_quarter_blurred_binding_desc,
                input_eighth_blurred_binding_desc,
                input_sixteenth_blurred_binding_desc,
                input_thirtysecond_blurred_binding_desc,
                input_mask_param_binding_desc,
                input_mask_param_secondary_binding_desc,
                input_panel_optics_binding_desc,
                input_panel_optics_secondary_binding_desc,
                input_panel_profile_binding_desc,
                input_panel_profile_secondary_binding_desc,
                input_history_binding_desc,
                input_velocity_binding_desc,
                output_binding_desc,
                history_output_binding_desc
            };
        }
        frosted_composite_pass_setup_info.buffer_resources["g_frosted_panels"] = panel_data_binding_desc;
        frosted_composite_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;
        frosted_composite_pass_setup_info.execute_command = make_compute_dispatch(width, height);
        return graph.CreateRenderGraphNode(resource_operator, frosted_composite_pass_setup_info);
    };

    m_frosted_composite_history_ab_pass_node =
        create_frosted_composite_node("Frosted Composite History A->B", m_temporal_history_a, m_temporal_history_b);
    m_frosted_composite_history_ba_pass_node =
        create_frosted_composite_node("Frosted Composite History B->A", m_temporal_history_b, m_temporal_history_a);

    UploadPanelData(resource_operator);
    graph.RegisterRenderTargetToColorOutput(m_frosted_pass_output);
    return true;
}

bool RendererSystemFrostedGlass::HasInit() const
{
    return m_downsample_half_pass_node != NULL_HANDLE &&
           m_blur_half_horizontal_pass_node != NULL_HANDLE &&
           m_blur_half_vertical_pass_node != NULL_HANDLE &&
           m_downsample_quarter_pass_node != NULL_HANDLE &&
           m_blur_quarter_horizontal_pass_node != NULL_HANDLE &&
           m_blur_quarter_vertical_pass_node != NULL_HANDLE &&
           m_downsample_eighth_pass_node != NULL_HANDLE &&
           m_blur_eighth_horizontal_pass_node != NULL_HANDLE &&
           m_blur_eighth_vertical_pass_node != NULL_HANDLE &&
           m_downsample_sixteenth_pass_node != NULL_HANDLE &&
           m_blur_sixteenth_horizontal_pass_node != NULL_HANDLE &&
           m_blur_sixteenth_vertical_pass_node != NULL_HANDLE &&
           m_downsample_thirtysecond_pass_node != NULL_HANDLE &&
           m_blur_thirtysecond_horizontal_pass_node != NULL_HANDLE &&
           m_blur_thirtysecond_vertical_pass_node != NULL_HANDLE &&
           m_frosted_mask_parameter_pass_node != NULL_HANDLE &&
           m_frosted_composite_back_pass_node != NULL_HANDLE &&
           m_downsample_half_multilayer_pass_node != NULL_HANDLE &&
           m_blur_half_multilayer_horizontal_pass_node != NULL_HANDLE &&
           m_blur_half_multilayer_vertical_pass_node != NULL_HANDLE &&
           m_downsample_quarter_multilayer_pass_node != NULL_HANDLE &&
           m_blur_quarter_multilayer_horizontal_pass_node != NULL_HANDLE &&
           m_blur_quarter_multilayer_vertical_pass_node != NULL_HANDLE &&
           m_downsample_eighth_multilayer_pass_node != NULL_HANDLE &&
           m_blur_eighth_multilayer_horizontal_pass_node != NULL_HANDLE &&
           m_blur_eighth_multilayer_vertical_pass_node != NULL_HANDLE &&
           m_downsample_sixteenth_multilayer_pass_node != NULL_HANDLE &&
           m_blur_sixteenth_multilayer_horizontal_pass_node != NULL_HANDLE &&
           m_blur_sixteenth_multilayer_vertical_pass_node != NULL_HANDLE &&
           m_downsample_thirtysecond_multilayer_pass_node != NULL_HANDLE &&
           m_blur_thirtysecond_multilayer_horizontal_pass_node != NULL_HANDLE &&
           m_blur_thirtysecond_multilayer_vertical_pass_node != NULL_HANDLE &&
           m_frosted_composite_front_history_ab_pass_node != NULL_HANDLE &&
           m_frosted_composite_front_history_ba_pass_node != NULL_HANDLE &&
           m_frosted_composite_history_ab_pass_node != NULL_HANDLE &&
           m_frosted_composite_history_ba_pass_node != NULL_HANDLE &&
           m_frosted_pass_output != NULL_HANDLE &&
           m_frosted_back_composite_output != NULL_HANDLE &&
           m_frosted_mask_parameter_output != NULL_HANDLE &&
            m_frosted_mask_parameter_secondary_output != NULL_HANDLE &&
            m_frosted_panel_optics_output != NULL_HANDLE &&
            m_frosted_panel_optics_secondary_output != NULL_HANDLE &&
            m_frosted_panel_profile_output != NULL_HANDLE &&
            m_frosted_panel_profile_secondary_output != NULL_HANDLE &&
            m_frosted_panel_payload_depth != NULL_HANDLE &&
            m_frosted_panel_payload_depth_secondary != NULL_HANDLE &&
            m_half_multilayer_ping != NULL_HANDLE &&
           m_half_multilayer_pong != NULL_HANDLE &&
           m_quarter_multilayer_ping != NULL_HANDLE &&
           m_quarter_multilayer_pong != NULL_HANDLE &&
           m_eighth_blur_ping != NULL_HANDLE &&
           m_eighth_blur_pong != NULL_HANDLE &&
           m_sixteenth_blur_ping != NULL_HANDLE &&
           m_sixteenth_blur_pong != NULL_HANDLE &&
           m_thirtysecond_blur_ping != NULL_HANDLE &&
           m_thirtysecond_blur_pong != NULL_HANDLE &&
           m_eighth_multilayer_ping != NULL_HANDLE &&
           m_eighth_multilayer_pong != NULL_HANDLE &&
           m_sixteenth_multilayer_ping != NULL_HANDLE &&
           m_sixteenth_multilayer_pong != NULL_HANDLE &&
           m_thirtysecond_multilayer_ping != NULL_HANDLE &&
           m_thirtysecond_multilayer_pong != NULL_HANDLE &&
           m_half_blur_final_output != NULL_HANDLE &&
           m_quarter_blur_final_output != NULL_HANDLE &&
           m_eighth_blur_final_output != NULL_HANDLE &&
           m_sixteenth_blur_final_output != NULL_HANDLE &&
           m_thirtysecond_blur_final_output != NULL_HANDLE &&
           m_half_multilayer_blur_final_output != NULL_HANDLE &&
           m_quarter_multilayer_blur_final_output != NULL_HANDLE &&
           m_eighth_multilayer_blur_final_output != NULL_HANDLE &&
           m_sixteenth_multilayer_blur_final_output != NULL_HANDLE &&
           m_thirtysecond_multilayer_blur_final_output != NULL_HANDLE &&
           m_temporal_history_a != NULL_HANDLE &&
           m_temporal_history_b != NULL_HANDLE &&
           m_postfx_shared_resources.HasInit();
}

bool RendererSystemFrostedGlass::Tick(RendererInterface::ResourceOperator& resource_operator,
                                      RendererInterface::RenderGraph& graph,
                                      unsigned long long interval)
{
    const float delta_seconds = static_cast<float>(interval) / 1000.0f;
    const unsigned width = (std::max)(1u, resource_operator.GetCurrentRenderWidth());
    const unsigned height = (std::max)(1u, resource_operator.GetCurrentRenderHeight());
    const unsigned half_width = (width + 1u) / 2u;
    const unsigned half_height = (height + 1u) / 2u;
    const unsigned quarter_width = (width + 3u) / 4u;
    const unsigned quarter_height = (height + 3u) / 4u;
    const unsigned eighth_width = (width + 7u) / 8u;
    const unsigned eighth_height = (height + 7u) / 8u;
    const unsigned sixteenth_width = (width + 15u) / 16u;
    const unsigned sixteenth_height = (height + 15u) / 16u;
    const unsigned thirtysecond_width = (width + 31u) / 32u;
    const unsigned thirtysecond_height = (height + 31u) / 32u;

    graph.UpdateComputeDispatch(m_downsample_half_pass_node, (half_width + 7) / 8, (half_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_blur_half_horizontal_pass_node, (half_width + 7) / 8, (half_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_blur_half_vertical_pass_node, (half_width + 7) / 8, (half_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_downsample_quarter_pass_node, (quarter_width + 7) / 8, (quarter_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_blur_quarter_horizontal_pass_node, (quarter_width + 7) / 8, (quarter_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_blur_quarter_vertical_pass_node, (quarter_width + 7) / 8, (quarter_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_downsample_eighth_pass_node, (eighth_width + 7) / 8, (eighth_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_blur_eighth_horizontal_pass_node, (eighth_width + 7) / 8, (eighth_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_blur_eighth_vertical_pass_node, (eighth_width + 7) / 8, (eighth_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_downsample_sixteenth_pass_node, (sixteenth_width + 7) / 8, (sixteenth_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_blur_sixteenth_horizontal_pass_node, (sixteenth_width + 7) / 8, (sixteenth_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_blur_sixteenth_vertical_pass_node, (sixteenth_width + 7) / 8, (sixteenth_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_downsample_thirtysecond_pass_node, (thirtysecond_width + 7) / 8, (thirtysecond_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_blur_thirtysecond_horizontal_pass_node, (thirtysecond_width + 7) / 8, (thirtysecond_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_blur_thirtysecond_vertical_pass_node, (thirtysecond_width + 7) / 8, (thirtysecond_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_frosted_mask_parameter_pass_node, (width + 7) / 8, (height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_frosted_composite_back_pass_node, (width + 7) / 8, (height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_downsample_half_multilayer_pass_node, (half_width + 7) / 8, (half_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_blur_half_multilayer_horizontal_pass_node, (half_width + 7) / 8, (half_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_blur_half_multilayer_vertical_pass_node, (half_width + 7) / 8, (half_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_downsample_quarter_multilayer_pass_node, (quarter_width + 7) / 8, (quarter_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_blur_quarter_multilayer_horizontal_pass_node, (quarter_width + 7) / 8, (quarter_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_blur_quarter_multilayer_vertical_pass_node, (quarter_width + 7) / 8, (quarter_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_downsample_eighth_multilayer_pass_node, (eighth_width + 7) / 8, (eighth_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_blur_eighth_multilayer_horizontal_pass_node, (eighth_width + 7) / 8, (eighth_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_blur_eighth_multilayer_vertical_pass_node, (eighth_width + 7) / 8, (eighth_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_downsample_sixteenth_multilayer_pass_node, (sixteenth_width + 7) / 8, (sixteenth_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_blur_sixteenth_multilayer_horizontal_pass_node, (sixteenth_width + 7) / 8, (sixteenth_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_blur_sixteenth_multilayer_vertical_pass_node, (sixteenth_width + 7) / 8, (sixteenth_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_downsample_thirtysecond_multilayer_pass_node, (thirtysecond_width + 7) / 8, (thirtysecond_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_blur_thirtysecond_multilayer_horizontal_pass_node, (thirtysecond_width + 7) / 8, (thirtysecond_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_blur_thirtysecond_multilayer_vertical_pass_node, (thirtysecond_width + 7) / 8, (thirtysecond_height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_frosted_composite_front_history_ab_pass_node, (width + 7) / 8, (height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_frosted_composite_front_history_ba_pass_node, (width + 7) / 8, (height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_frosted_composite_history_ab_pass_node, (width + 7) / 8, (height + 7) / 8, 1);
    graph.UpdateComputeDispatch(m_frosted_composite_history_ba_pass_node, (width + 7) / 8, (height + 7) / 8, 1);

    const auto camera_module = m_scene->GetCameraModule();
    const bool camera_invalidation_requested = camera_module && camera_module->ConsumeTemporalHistoryInvalidation();
    const bool temporal_invalidation_requested = m_temporal_force_reset || camera_invalidation_requested;
    if (temporal_invalidation_requested)
    {
        m_temporal_history_valid = false;
    }
    m_temporal_force_reset = false;

    const unsigned history_valid_flag = m_temporal_history_valid ? 1u : 0u;
    if (m_global_params.temporal_history_valid != history_valid_flag)
    {
        m_global_params.temporal_history_valid = history_valid_flag;
        m_need_upload_global_params = true;
    }

    bool multilayer_runtime_enabled = false;
    if (m_global_params.multilayer_mode == MULTILAYER_MODE_SINGLE)
    {
        m_multilayer_over_budget_streak = 0;
        m_multilayer_cooldown_frames = 0;
        multilayer_runtime_enabled = false;
    }
    else if (m_global_params.multilayer_mode == MULTILAYER_MODE_FORCE)
    {
        m_multilayer_over_budget_streak = 0;
        m_multilayer_cooldown_frames = 0;
        multilayer_runtime_enabled = true;
    }
    else
    {
        const float frame_ms = static_cast<float>(interval);
        const float frame_budget_ms = (std::max)(m_global_params.multilayer_frame_budget_ms, 1.0f);
        if (m_multilayer_cooldown_frames > 0)
        {
            --m_multilayer_cooldown_frames;
            m_multilayer_over_budget_streak = 0;
            multilayer_runtime_enabled = false;
        }
        else
        {
            if (frame_ms > frame_budget_ms)
            {
                ++m_multilayer_over_budget_streak;
            }
            else if (m_multilayer_over_budget_streak > 0)
            {
                --m_multilayer_over_budget_streak;
            }

            if (m_multilayer_over_budget_streak >= MULTILAYER_AUTO_OVER_BUDGET_TRIGGER_FRAMES)
            {
                m_multilayer_over_budget_streak = 0;
                m_multilayer_cooldown_frames = MULTILAYER_AUTO_COOLDOWN_FRAMES;
                multilayer_runtime_enabled = false;
            }
            else
            {
                multilayer_runtime_enabled = true;
            }
        }
    }

    if (m_multilayer_runtime_enabled != multilayer_runtime_enabled)
    {
        m_multilayer_runtime_enabled = multilayer_runtime_enabled;
        m_need_upload_global_params = true;
    }
    const unsigned multilayer_runtime_flag = m_multilayer_runtime_enabled ? 1u : 0u;
    if (m_global_params.multilayer_runtime_enabled != multilayer_runtime_flag)
    {
        m_global_params.multilayer_runtime_enabled = multilayer_runtime_flag;
        m_need_upload_global_params = true;
    }

    UpdatePanelRuntimeStates(delta_seconds);
    UploadPanelData(resource_operator);

    const RendererInterface::RenderGraphNodeHandle active_legacy_composite_pass =
        m_temporal_history_read_is_a ? m_frosted_composite_history_ab_pass_node : m_frosted_composite_history_ba_pass_node;
    const RendererInterface::RenderGraphNodeHandle active_front_composite_pass =
        m_temporal_history_read_is_a ? m_frosted_composite_front_history_ab_pass_node : m_frosted_composite_front_history_ba_pass_node;
    const bool request_raster_panel_payload = m_panel_payload_path == PanelPayloadPath::RasterPanelGBuffer;
    const bool use_raster_panel_payload = request_raster_panel_payload && m_panel_payload_raster_ready;
    m_panel_payload_compute_fallback_active = request_raster_panel_payload && !use_raster_panel_payload;
    const bool use_strict_multilayer_path =
        m_global_params.multilayer_mode == MULTILAYER_MODE_FORCE &&
        m_multilayer_runtime_enabled;

    graph.RegisterRenderGraphNode(m_downsample_half_pass_node);
    graph.RegisterRenderGraphNode(m_blur_half_horizontal_pass_node);
    graph.RegisterRenderGraphNode(m_blur_half_vertical_pass_node);
    graph.RegisterRenderGraphNode(m_downsample_quarter_pass_node);
    graph.RegisterRenderGraphNode(m_blur_quarter_horizontal_pass_node);
    graph.RegisterRenderGraphNode(m_blur_quarter_vertical_pass_node);
    graph.RegisterRenderGraphNode(m_downsample_eighth_pass_node);
    graph.RegisterRenderGraphNode(m_blur_eighth_horizontal_pass_node);
    graph.RegisterRenderGraphNode(m_blur_eighth_vertical_pass_node);
    graph.RegisterRenderGraphNode(m_downsample_sixteenth_pass_node);
    graph.RegisterRenderGraphNode(m_blur_sixteenth_horizontal_pass_node);
    graph.RegisterRenderGraphNode(m_blur_sixteenth_vertical_pass_node);
    graph.RegisterRenderGraphNode(m_downsample_thirtysecond_pass_node);
    graph.RegisterRenderGraphNode(m_blur_thirtysecond_horizontal_pass_node);
    graph.RegisterRenderGraphNode(m_blur_thirtysecond_vertical_pass_node);
    if (use_raster_panel_payload)
    {
        graph.RegisterRenderGraphNode(m_frosted_mask_parameter_raster_front_pass_node);
        graph.RegisterRenderGraphNode(m_frosted_mask_parameter_raster_back_pass_node);
    }
    else
    {
        graph.RegisterRenderGraphNode(m_frosted_mask_parameter_pass_node);
    }
    if (use_strict_multilayer_path)
    {
        graph.RegisterRenderGraphNode(m_frosted_composite_back_pass_node);
        graph.RegisterRenderGraphNode(m_downsample_half_multilayer_pass_node);
        graph.RegisterRenderGraphNode(m_blur_half_multilayer_horizontal_pass_node);
        graph.RegisterRenderGraphNode(m_blur_half_multilayer_vertical_pass_node);
        graph.RegisterRenderGraphNode(m_downsample_quarter_multilayer_pass_node);
        graph.RegisterRenderGraphNode(m_blur_quarter_multilayer_horizontal_pass_node);
        graph.RegisterRenderGraphNode(m_blur_quarter_multilayer_vertical_pass_node);
        graph.RegisterRenderGraphNode(m_downsample_eighth_multilayer_pass_node);
        graph.RegisterRenderGraphNode(m_blur_eighth_multilayer_horizontal_pass_node);
        graph.RegisterRenderGraphNode(m_blur_eighth_multilayer_vertical_pass_node);
        graph.RegisterRenderGraphNode(m_downsample_sixteenth_multilayer_pass_node);
        graph.RegisterRenderGraphNode(m_blur_sixteenth_multilayer_horizontal_pass_node);
        graph.RegisterRenderGraphNode(m_blur_sixteenth_multilayer_vertical_pass_node);
        graph.RegisterRenderGraphNode(m_downsample_thirtysecond_multilayer_pass_node);
        graph.RegisterRenderGraphNode(m_blur_thirtysecond_multilayer_horizontal_pass_node);
        graph.RegisterRenderGraphNode(m_blur_thirtysecond_multilayer_vertical_pass_node);
        graph.RegisterRenderGraphNode(active_front_composite_pass);
    }
    else
    {
        graph.RegisterRenderGraphNode(active_legacy_composite_pass);
    }
    graph.RegisterRenderTargetToColorOutput(m_frosted_pass_output);

    const bool next_history_valid = !m_panel_descs.empty();
    if (m_temporal_history_valid != next_history_valid)
    {
        m_temporal_history_valid = next_history_valid;
        m_need_upload_global_params = true;
    }
    m_temporal_history_read_is_a = !m_temporal_history_read_is_a;
    return true;
}

void RendererSystemFrostedGlass::OnResize(RendererInterface::ResourceOperator& resource_operator, unsigned width, unsigned height)
{
    (void)resource_operator;
    (void)width;
    (void)height;
    m_temporal_force_reset = true;
    m_temporal_history_valid = false;
    m_temporal_history_read_is_a = true;
    m_global_params.temporal_history_valid = 0;
    m_multilayer_over_budget_streak = 0;
    m_multilayer_cooldown_frames = 0;
    m_need_upload_global_params = true;
}

void RendererSystemFrostedGlass::DrawDebugUI()
{
    bool panel_dirty = false;
    bool global_dirty = false;

    int blur_radius = static_cast<int>(m_global_params.blur_radius);
    if (ImGui::SliderInt("Blur Radius", &blur_radius, 1, 24))
    {
        m_global_params.blur_radius = static_cast<unsigned>(blur_radius);
        global_dirty = true;
    }
    if (ImGui::SliderFloat("Blur Response Scale", &m_global_params.blur_response_scale, 0.6f, 2.5f, "%.2f"))
    {
        global_dirty = true;
    }
    if (ImGui::SliderFloat("Blur Veil Strength", &m_global_params.blur_veil_strength, 0.0f, 2.0f, "%.2f"))
    {
        global_dirty = true;
    }
    if (ImGui::SliderFloat("Blur Detail Preservation", &m_global_params.blur_detail_preservation, 0.0f, 1.0f, "%.2f"))
    {
        global_dirty = true;
    }
    const char* multilayer_modes[] = {"Single", "Auto", "MultiLayer"};
    int multilayer_mode = static_cast<int>(m_global_params.multilayer_mode);
    if (ImGui::Combo("Multilayer Mode", &multilayer_mode, multilayer_modes, IM_ARRAYSIZE(multilayer_modes)))
    {
        multilayer_mode = (std::max)(0, (std::min)(multilayer_mode, 2));
        m_global_params.multilayer_mode = static_cast<unsigned>(multilayer_mode);
        m_multilayer_over_budget_streak = 0;
        m_multilayer_cooldown_frames = 0;
        global_dirty = true;
    }
    if (ImGui::SliderFloat("Multilayer Back Weight", &m_global_params.multilayer_back_layer_weight, 0.0f, 1.0f, "%.2f"))
    {
        global_dirty = true;
    }
    if (ImGui::SliderFloat("Multilayer Front Transmittance", &m_global_params.multilayer_front_transmittance, 0.0f, 1.0f, "%.2f"))
    {
        global_dirty = true;
    }
    if (ImGui::CollapsingHeader("Advanced / Global"))
    {
        const char* panel_payload_paths[] = {"Compute (SDF)", "Raster (Panel GBuffer)"};
        int panel_payload_path = static_cast<int>(m_panel_payload_path);
        if (ImGui::Combo("Panel Payload Path", &panel_payload_path, panel_payload_paths, IM_ARRAYSIZE(panel_payload_paths)))
        {
            panel_payload_path = (std::max)(0, (std::min)(panel_payload_path, 1));
            m_panel_payload_path = static_cast<PanelPayloadPath>(panel_payload_path);
        }
        if (ImGui::SliderFloat("Blur Kernel Sigma Scale", &m_global_params.blur_kernel_sigma_scale, 0.6f, 3.5f, "%.2f"))
        {
            global_dirty = true;
        }
        if (ImGui::SliderFloat("Blur Quarter Mix Boost", &m_global_params.blur_quarter_mix_boost, 0.0f, 1.0f, "%.2f"))
        {
            global_dirty = true;
        }
        if (ImGui::SliderFloat("Blur Sigma Normalization", &m_global_params.blur_sigma_normalization, 4.0f, 12.0f, "%.2f"))
        {
            global_dirty = true;
        }
        if (ImGui::SliderFloat("Depth Aware Min Strength", &m_global_params.depth_aware_min_strength, 0.0f, 1.0f, "%.2f"))
        {
            global_dirty = true;
        }
        if (ImGui::SliderFloat("Blur Contrast Compression", &m_global_params.blur_contrast_compression, 0.0f, 1.0f, "%.2f"))
        {
            global_dirty = true;
        }
        if (ImGui::SliderFloat("Blur Veil Tint Mix", &m_global_params.blur_veil_tint_mix, 0.0f, 1.0f, "%.2f"))
        {
            global_dirty = true;
        }
        if (ImGui::SliderFloat("Scene Edge Scale", &m_global_params.scene_edge_scale, 0.0f, 120.0f, "%.1f"))
        {
            global_dirty = true;
        }
        if (ImGui::SliderFloat("Temporal History Blend", &m_global_params.temporal_history_blend, 0.0f, 0.98f, "%.2f"))
        {
            global_dirty = true;
        }
        if (ImGui::SliderFloat("Temporal Velocity Reject", &m_global_params.temporal_reject_velocity, 0.001f, 0.20f, "%.3f"))
        {
            global_dirty = true;
        }
        if (ImGui::SliderFloat("Temporal Edge Reject", &m_global_params.temporal_edge_reject, 0.0f, 2.0f, "%.2f"))
        {
            global_dirty = true;
        }
        if (ImGui::SliderFloat("Multilayer Frame Budget (ms)", &m_global_params.multilayer_frame_budget_ms, 8.0f, 50.0f, "%.1f"))
        {
            global_dirty = true;
        }
        if (ImGui::SliderFloat("Multilayer Overlap Threshold", &m_global_params.multilayer_overlap_threshold, 0.0f, 0.80f, "%.2f"))
        {
            global_dirty = true;
        }
    }
    if (ImGui::Button("Reset Temporal History"))
    {
        m_temporal_force_reset = true;
        m_temporal_history_valid = false;
        m_global_params.temporal_history_valid = 0;
        global_dirty = true;
    }
    ImGui::Text("Temporal History: %s | Read Buffer: %s",
                m_temporal_history_valid ? "Valid" : "Invalid",
                m_temporal_history_read_is_a ? "A" : "B");
    ImGui::Text("Multilayer Runtime: %s | Cooldown: %u | OverBudgetStreak: %u",
                m_multilayer_runtime_enabled ? "Enabled" : "Disabled",
                m_multilayer_cooldown_frames,
                m_multilayer_over_budget_streak);
    const bool strict_multilayer_path_active =
        m_global_params.multilayer_mode == MULTILAYER_MODE_FORCE &&
        m_multilayer_runtime_enabled;
    ImGui::Text("Multilayer Path: %s",
                strict_multilayer_path_active ? "Strict Sequential (Back->Front)" : "Fast Single-Pass");
    const bool using_raster_panel_payload = m_panel_payload_path == PanelPayloadPath::RasterPanelGBuffer;
    ImGui::Text("Panel Payload Path: %s",
                using_raster_panel_payload ? "Raster (Panel GBuffer)" : "Compute (SDF)");
    if (m_panel_payload_compute_fallback_active)
    {
        ImGui::TextUnformatted("Panel Payload Runtime: Raster requested but not ready; using Compute fallback.");
    }

    if (m_panel_descs.empty())
    {
        ImGui::TextUnformatted("No frosted panels.");
        if (panel_dirty)
        {
            m_need_upload_panels = true;
        }
        if (global_dirty)
        {
            m_need_upload_global_params = true;
        }
        return;
    }

    int selected_panel_index = static_cast<int>(m_debug_selected_panel_index);
    const int max_panel_index = static_cast<int>(m_panel_descs.size()) - 1;
    selected_panel_index = (std::max)(0, (std::min)(selected_panel_index, max_panel_index));
    if (ImGui::SliderInt("Panel Index", &selected_panel_index, 0, max_panel_index))
    {
        m_debug_selected_panel_index = static_cast<unsigned>(selected_panel_index);
    }
    else
    {
        m_debug_selected_panel_index = static_cast<unsigned>(selected_panel_index);
    }

    auto& panel = m_panel_descs[m_debug_selected_panel_index];

    if (ImGui::SliderFloat2("Center UV", &panel.center_uv.x, 0.0f, 1.0f, "%.3f"))
    {
        panel_dirty = true;
    }
    if (ImGui::SliderFloat2("Half Size UV", &panel.half_size_uv.x, 0.02f, 0.48f, "%.3f"))
    {
        panel_dirty = true;
    }

    int shape_type = static_cast<int>(panel.shape_type);
    const char* shape_types[] = {"RoundedRect", "Circle", "ShapeMask"};
    if (ImGui::Combo("Shape Type", &shape_type, shape_types, IM_ARRAYSIZE(shape_types)))
    {
        panel.shape_type = static_cast<PanelShapeType>(shape_type);
        panel_dirty = true;
    }
    if (ImGui::SliderFloat("Layer Order", &panel.layer_order, -8.0f, 8.0f, "%.1f"))
    {
        panel_dirty = true;
    }

    const char* interaction_states[] = {"Idle", "Hover", "Grab", "Move", "Scale"};
    int interaction_state = static_cast<int>(panel.interaction_state);
    if (ImGui::Combo("Interaction State", &interaction_state, interaction_states, IM_ARRAYSIZE(interaction_states)))
    {
        interaction_state = (std::max)(0, (std::min)(interaction_state, static_cast<int>(PanelInteractionState::Count) - 1));
        panel.interaction_state = static_cast<PanelInteractionState>(interaction_state);
        panel_dirty = true;
    }
    if (ImGui::SliderFloat("Panel Alpha", &panel.panel_alpha, 0.0f, 1.0f, "%.2f"))
    {
        panel_dirty = true;
    }

    const float max_corner_radius = (std::max)(0.0f, (std::min)(panel.half_size_uv.x, panel.half_size_uv.y) - 0.001f);
    if (ImGui::SliderFloat("Blur Sigma", &panel.blur_sigma, 0.2f, 24.0f, "%.2f"))
    {
        panel_dirty = true;
    }
    if (ImGui::SliderFloat("Blur Strength", &panel.blur_strength, 0.0f, 1.0f, "%.2f"))
    {
        panel_dirty = true;
    }
    if (ImGui::ColorEdit3("Tint Color", &panel.tint_color.x))
    {
        panel_dirty = true;
    }
    if (ImGui::SliderFloat("Fresnel Intensity", &panel.fresnel_intensity, 0.0f, 0.6f, "%.3f"))
    {
        panel_dirty = true;
    }
    if (ImGui::CollapsingHeader("Advanced / Panel"))
    {
        if (ImGui::SliderFloat("Custom Shape Index", &panel.custom_shape_index, 0.0f, 7.0f, "%.1f"))
        {
            panel_dirty = true;
        }
        if (ImGui::SliderFloat("State Transition Speed", &panel.interaction_transition_speed, 1.0f, 24.0f, "%.2f"))
        {
            panel_dirty = true;
        }
        if (ImGui::SliderFloat("Corner Radius", &panel.corner_radius, 0.0f, max_corner_radius, "%.3f"))
        {
            panel_dirty = true;
        }
        if (ImGui::SliderFloat("Rim Intensity", &panel.rim_intensity, 0.0f, 0.6f, "%.3f"))
        {
            panel_dirty = true;
        }
        if (ImGui::SliderFloat("Depth Weight Scale", &panel.depth_weight_scale, 1.0f, 200.0f, "%.1f"))
        {
            panel_dirty = true;
        }
        if (ImGui::SliderFloat("Edge Softness", &panel.edge_softness, 0.1f, 4.0f, "%.2f"))
        {
            panel_dirty = true;
        }
        if (ImGui::SliderFloat("Thickness", &panel.thickness, 0.0f, 0.10f, "%.3f"))
        {
            panel_dirty = true;
        }
        if (ImGui::SliderFloat("Refraction Strength", &panel.refraction_strength, 0.0f, 4.0f, "%.2f"))
        {
            panel_dirty = true;
        }
        if (ImGui::SliderFloat("Fresnel Power", &panel.fresnel_power, 1.0f, 10.0f, "%.2f"))
        {
            panel_dirty = true;
        }

        int curve_state = static_cast<int>(m_debug_selected_curve_state_index);
        curve_state = (std::max)(0, (std::min)(curve_state, static_cast<int>(PanelInteractionState::Count) - 1));
        if (ImGui::Combo("Edit State Curve", &curve_state, interaction_states, IM_ARRAYSIZE(interaction_states)))
        {
            m_debug_selected_curve_state_index = static_cast<unsigned>(curve_state);
        }
        else
        {
            m_debug_selected_curve_state_index = static_cast<unsigned>(curve_state);
        }
        auto& curve = panel.state_curves[m_debug_selected_curve_state_index];
        if (ImGui::SliderFloat("Curve Blur Sigma Scale", &curve.blur_sigma_scale, 0.4f, 2.5f, "%.2f"))
        {
            panel_dirty = true;
        }
        if (ImGui::SliderFloat("Curve Blur Strength Scale", &curve.blur_strength_scale, 0.4f, 2.5f, "%.2f"))
        {
            panel_dirty = true;
        }
        if (ImGui::SliderFloat("Curve Rim Scale", &curve.rim_intensity_scale, 0.4f, 2.5f, "%.2f"))
        {
            panel_dirty = true;
        }
        if (ImGui::SliderFloat("Curve Fresnel Scale", &curve.fresnel_intensity_scale, 0.4f, 2.5f, "%.2f"))
        {
            panel_dirty = true;
        }
        if (ImGui::SliderFloat("Curve Alpha Scale", &curve.alpha_scale, 0.0f, 1.5f, "%.2f"))
        {
            panel_dirty = true;
        }
    }

    if (panel_dirty)
    {
        const auto clamp = [](float value, float min_value, float max_value) -> float
        {
            return (std::max)(min_value, (std::min)(value, max_value));
        };

        const float center_min_x = panel.half_size_uv.x;
        const float center_min_y = panel.half_size_uv.y;
        const float center_max_x = 1.0f - panel.half_size_uv.x;
        const float center_max_y = 1.0f - panel.half_size_uv.y;
        panel.center_uv.x = clamp(panel.center_uv.x, center_min_x, center_max_x);
        panel.center_uv.y = clamp(panel.center_uv.y, center_min_y, center_max_y);
        panel.corner_radius = clamp(panel.corner_radius, 0.0f, max_corner_radius);
        panel.blur_sigma = clamp(panel.blur_sigma, 0.2f, 24.0f);
        panel.blur_strength = clamp(panel.blur_strength, 0.0f, 1.0f);
        panel.rim_intensity = clamp(panel.rim_intensity, 0.0f, 1.0f);
        panel.edge_softness = clamp(panel.edge_softness, 0.1f, 8.0f);
        panel.thickness = clamp(panel.thickness, 0.0f, 0.2f);
        panel.refraction_strength = clamp(panel.refraction_strength, 0.0f, 8.0f);
        panel.fresnel_intensity = clamp(panel.fresnel_intensity, 0.0f, 1.0f);
        panel.fresnel_power = clamp(panel.fresnel_power, 1.0f, 16.0f);
        panel.custom_shape_index = clamp(panel.custom_shape_index, 0.0f, 7.0f);
        panel.layer_order = clamp(panel.layer_order, -8.0f, 8.0f);
        panel.panel_alpha = clamp(panel.panel_alpha, 0.0f, 1.0f);
        panel.interaction_transition_speed = clamp(panel.interaction_transition_speed, 1.0f, 24.0f);
        for (auto& state_curve : panel.state_curves)
        {
            state_curve.blur_sigma_scale = clamp(state_curve.blur_sigma_scale, 0.4f, 2.5f);
            state_curve.blur_strength_scale = clamp(state_curve.blur_strength_scale, 0.4f, 2.5f);
            state_curve.rim_intensity_scale = clamp(state_curve.rim_intensity_scale, 0.4f, 2.5f);
            state_curve.fresnel_intensity_scale = clamp(state_curve.fresnel_intensity_scale, 0.4f, 2.5f);
            state_curve.alpha_scale = clamp(state_curve.alpha_scale, 0.0f, 1.5f);
        }
        m_need_upload_panels = true;
    }
    if (global_dirty)
    {
        const auto clamp = [](float value, float min_value, float max_value) -> float
        {
            return (std::max)(min_value, (std::min)(value, max_value));
        };
        m_global_params.temporal_history_blend = clamp(m_global_params.temporal_history_blend, 0.0f, 0.98f);
        m_global_params.temporal_reject_velocity = clamp(m_global_params.temporal_reject_velocity, 0.001f, 0.20f);
        m_global_params.temporal_edge_reject = clamp(m_global_params.temporal_edge_reject, 0.0f, 2.0f);
        m_global_params.blur_kernel_sigma_scale = clamp(m_global_params.blur_kernel_sigma_scale, 0.6f, 3.5f);
        m_global_params.blur_quarter_mix_boost = clamp(m_global_params.blur_quarter_mix_boost, 0.0f, 1.0f);
        m_global_params.blur_response_scale = clamp(m_global_params.blur_response_scale, 0.6f, 2.5f);
        m_global_params.blur_sigma_normalization = clamp(m_global_params.blur_sigma_normalization, 4.0f, 12.0f);
        m_global_params.depth_aware_min_strength = clamp(m_global_params.depth_aware_min_strength, 0.0f, 1.0f);
        m_global_params.blur_veil_strength = clamp(m_global_params.blur_veil_strength, 0.0f, 2.0f);
        m_global_params.blur_contrast_compression = clamp(m_global_params.blur_contrast_compression, 0.0f, 1.0f);
        m_global_params.blur_veil_tint_mix = clamp(m_global_params.blur_veil_tint_mix, 0.0f, 1.0f);
        m_global_params.blur_detail_preservation = clamp(m_global_params.blur_detail_preservation, 0.0f, 1.0f);
        m_global_params.multilayer_frame_budget_ms = clamp(m_global_params.multilayer_frame_budget_ms, 8.0f, 50.0f);
        m_global_params.multilayer_overlap_threshold = clamp(m_global_params.multilayer_overlap_threshold, 0.0f, 0.80f);
        m_global_params.multilayer_back_layer_weight = clamp(m_global_params.multilayer_back_layer_weight, 0.0f, 1.0f);
        m_global_params.multilayer_front_transmittance = clamp(m_global_params.multilayer_front_transmittance, 0.0f, 1.0f);
        m_global_params.multilayer_mode = static_cast<unsigned>((std::max)(0, (std::min)(static_cast<int>(m_global_params.multilayer_mode), 2)));
        m_need_upload_global_params = true;
    }
}

void RendererSystemFrostedGlass::UpdatePanelRuntimeStates(float delta_seconds)
{
    if (m_panel_runtime_states.size() != m_panel_descs.size())
    {
        m_panel_runtime_states.resize(m_panel_descs.size());
        for (unsigned panel_index = 0; panel_index < m_panel_descs.size(); ++panel_index)
        {
            auto& runtime_state = m_panel_runtime_states[panel_index];
            runtime_state.state_weights.fill(0.0f);
            runtime_state.target_state = m_panel_descs[panel_index].interaction_state;
            runtime_state.state_weights[ToInteractionStateIndex(runtime_state.target_state)] = 1.0f;
        }
        m_need_upload_panels = true;
    }

    const float safe_delta_seconds = (std::max)(0.0f, delta_seconds);
    bool state_weight_dirty = false;
    for (unsigned panel_index = 0; panel_index < m_panel_descs.size(); ++panel_index)
    {
        const auto& panel_desc = m_panel_descs[panel_index];
        auto& runtime_state = m_panel_runtime_states[panel_index];
        runtime_state.target_state = panel_desc.interaction_state;

        const unsigned target_state_index = ToInteractionStateIndex(runtime_state.target_state);
        const float transition_speed = (std::max)(panel_desc.interaction_transition_speed, 1.0f);
        const float blend_alpha = 1.0f - std::exp(-transition_speed * safe_delta_seconds);

        float total_weight = 0.0f;
        for (unsigned state_index = 0; state_index < PANEL_INTERACTION_STATE_COUNT; ++state_index)
        {
            const float target_weight = state_index == target_state_index ? 1.0f : 0.0f;
            const float current_weight = runtime_state.state_weights[state_index];
            const float updated_weight = current_weight + (target_weight - current_weight) * blend_alpha;
            if (std::abs(updated_weight - current_weight) > 1e-4f)
            {
                state_weight_dirty = true;
            }
            runtime_state.state_weights[state_index] = updated_weight;
            total_weight += updated_weight;
        }

        if (total_weight > 1e-6f)
        {
            for (auto& weight : runtime_state.state_weights)
            {
                weight /= total_weight;
            }
        }
        else
        {
            runtime_state.state_weights.fill(0.0f);
            runtime_state.state_weights[target_state_index] = 1.0f;
            state_weight_dirty = true;
        }
    }

    if (state_weight_dirty)
    {
        m_need_upload_panels = true;
    }
}

RendererSystemFrostedGlass::PanelStateCurve RendererSystemFrostedGlass::GetBlendedStateCurve(unsigned panel_index) const
{
    PanelStateCurve blended_state_curve{};
    blended_state_curve.blur_sigma_scale = 0.0f;
    blended_state_curve.blur_strength_scale = 0.0f;
    blended_state_curve.rim_intensity_scale = 0.0f;
    blended_state_curve.fresnel_intensity_scale = 0.0f;
    blended_state_curve.alpha_scale = 0.0f;

    GLTF_CHECK(panel_index < m_panel_descs.size());
    GLTF_CHECK(panel_index < m_panel_runtime_states.size());
    const auto& panel_desc = m_panel_descs[panel_index];
    const auto& runtime_state = m_panel_runtime_states[panel_index];

    float total_weight = 0.0f;
    for (unsigned state_index = 0; state_index < PANEL_INTERACTION_STATE_COUNT; ++state_index)
    {
        const float weight = (std::max)(0.0f, runtime_state.state_weights[state_index]);
        const auto& state_curve = panel_desc.state_curves[state_index];
        blended_state_curve.blur_sigma_scale += state_curve.blur_sigma_scale * weight;
        blended_state_curve.blur_strength_scale += state_curve.blur_strength_scale * weight;
        blended_state_curve.rim_intensity_scale += state_curve.rim_intensity_scale * weight;
        blended_state_curve.fresnel_intensity_scale += state_curve.fresnel_intensity_scale * weight;
        blended_state_curve.alpha_scale += state_curve.alpha_scale * weight;
        total_weight += weight;
    }

    if (total_weight <= 1e-6f)
    {
        return panel_desc.state_curves[ToInteractionStateIndex(panel_desc.interaction_state)];
    }

    const float inv_total_weight = 1.0f / total_weight;
    blended_state_curve.blur_sigma_scale *= inv_total_weight;
    blended_state_curve.blur_strength_scale *= inv_total_weight;
    blended_state_curve.rim_intensity_scale *= inv_total_weight;
    blended_state_curve.fresnel_intensity_scale *= inv_total_weight;
    blended_state_curve.alpha_scale *= inv_total_weight;
    return blended_state_curve;
}

void RendererSystemFrostedGlass::UploadPanelData(RendererInterface::ResourceOperator& resource_operator)
{
    if (!m_need_upload_panels && !m_need_upload_global_params)
    {
        return;
    }

    if (m_need_upload_panels)
    {
        std::vector<FrostedGlassPanelGpuData> panel_gpu_datas;
        panel_gpu_datas.reserve(m_panel_descs.size());
        for (unsigned panel_index = 0; panel_index < m_panel_descs.size(); ++panel_index)
        {
            const auto blended_state_curve = GetBlendedStateCurve(panel_index);
            panel_gpu_datas.push_back(ConvertPanelToGpuData(m_panel_descs[panel_index], blended_state_curve));
        }

        if (!panel_gpu_datas.empty())
        {
            RendererInterface::BufferUploadDesc panel_data_upload_desc{};
            panel_data_upload_desc.data = panel_gpu_datas.data();
            panel_data_upload_desc.size = panel_gpu_datas.size() * sizeof(FrostedGlassPanelGpuData);
            resource_operator.UploadBufferData(m_frosted_panel_data_handle, panel_data_upload_desc);
        }

        m_global_params.panel_count = static_cast<unsigned>(panel_gpu_datas.size());
        m_need_upload_global_params = true;
    }

    if (m_need_upload_global_params)
    {
        RendererInterface::BufferUploadDesc global_params_upload_desc{};
        global_params_upload_desc.data = &m_global_params;
        global_params_upload_desc.size = sizeof(FrostedGlassGlobalParams);
        resource_operator.UploadBufferData(m_frosted_global_params_handle, global_params_upload_desc);
    }

    m_need_upload_panels = false;
    m_need_upload_global_params = false;
}

RendererSystemFrostedGlass::FrostedGlassPanelGpuData RendererSystemFrostedGlass::ConvertPanelToGpuData(
    const FrostedGlassPanelDesc& panel_desc, const PanelStateCurve& blended_state_curve) const
{
    const auto clamp = [](float value, float min_value, float max_value) -> float
    {
        return (std::max)(min_value, (std::min)(value, max_value));
    };

    const float dynamic_blur_sigma = clamp(
        panel_desc.blur_sigma * (std::max)(0.0f, blended_state_curve.blur_sigma_scale),
        0.2f,
        24.0f);
    const float dynamic_blur_strength = clamp(
        panel_desc.blur_strength * (std::max)(0.0f, blended_state_curve.blur_strength_scale),
        0.0f,
        1.0f);
    const float dynamic_rim_intensity = clamp(
        panel_desc.rim_intensity * (std::max)(0.0f, blended_state_curve.rim_intensity_scale),
        0.0f,
        1.0f);
    const float dynamic_fresnel_intensity = clamp(
        panel_desc.fresnel_intensity * (std::max)(0.0f, blended_state_curve.fresnel_intensity_scale),
        0.0f,
        1.0f);
    const float dynamic_panel_alpha = clamp(
        panel_desc.panel_alpha * (std::max)(0.0f, blended_state_curve.alpha_scale),
        0.0f,
        1.0f);

    FrostedGlassPanelGpuData gpu_data{};
    gpu_data.center_half_size = {
        panel_desc.center_uv.x,
        panel_desc.center_uv.y,
        panel_desc.half_size_uv.x,
        panel_desc.half_size_uv.y
    };
    gpu_data.corner_blur_rim = {
        panel_desc.corner_radius,
        dynamic_blur_sigma,
        dynamic_blur_strength,
        dynamic_rim_intensity
    };
    gpu_data.tint_depth_weight = {
        panel_desc.tint_color.x,
        panel_desc.tint_color.y,
        panel_desc.tint_color.z,
        panel_desc.depth_weight_scale
    };
    gpu_data.shape_info = {
        static_cast<float>(panel_desc.shape_type),
        panel_desc.edge_softness,
        panel_desc.custom_shape_index,
        dynamic_panel_alpha
    };
    gpu_data.optical_info = {
        panel_desc.thickness,
        panel_desc.refraction_strength,
        dynamic_fresnel_intensity,
        panel_desc.fresnel_power
    };
    gpu_data.layering_info = {
        panel_desc.layer_order,
        0.0f,
        0.0f,
        0.0f
    };
    return gpu_data;
}
