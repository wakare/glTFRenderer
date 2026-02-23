#include "RendererSystemToneMap.h"

#include <algorithm>
#include <imgui/imgui.h>

RendererSystemToneMap::RendererSystemToneMap(std::shared_ptr<RendererSystemFrostedGlass> frosted)
    : m_frosted(std::move(frosted))
{
}

bool RendererSystemToneMap::Init(RendererInterface::ResourceOperator& resource_operator,
                                 RendererInterface::RenderGraph& graph)
{
    GLTF_CHECK(m_frosted);
    GLTF_CHECK(m_frosted->HasInit());

    m_tone_map_output = resource_operator.CreateWindowRelativeRenderTarget(
        "ToneMap_Output",
        RendererInterface::RGBA8_UNORM,
        RendererInterface::default_clear_color,
        static_cast<RendererInterface::ResourceUsage>(
            RendererInterface::ResourceUsage::RENDER_TARGET |
            RendererInterface::ResourceUsage::COPY_SRC |
            RendererInterface::ResourceUsage::SHADER_RESOURCE |
            RendererInterface::ResourceUsage::UNORDER_ACCESS));

    RendererInterface::BufferDesc global_params_buffer_desc{};
    global_params_buffer_desc.name = "ToneMapGlobalBuffer";
    global_params_buffer_desc.size = sizeof(ToneMapGlobalParams);
    global_params_buffer_desc.type = RendererInterface::DEFAULT;
    global_params_buffer_desc.usage = RendererInterface::USAGE_CBV;
    m_tone_map_global_params_handle = resource_operator.CreateBuffer(global_params_buffer_desc);

    RendererInterface::RenderGraph::RenderPassSetupInfo tone_map_pass_setup_info{};
    tone_map_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    tone_map_pass_setup_info.debug_group = "Tone Map";
    tone_map_pass_setup_info.debug_name = "Tone Map Composite";
    tone_map_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "main", "Resources/Shaders/ToneMap.hlsl"}
    };

    RendererInterface::RenderTargetTextureBindingDesc input_color_binding_desc{};
    input_color_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
    input_color_binding_desc.name = "InputColorTex";
    input_color_binding_desc.render_target_texture = {m_frosted->GetOutput()};

    RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
    output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
    output_binding_desc.name = "Output";
    output_binding_desc.render_target_texture = {m_tone_map_output};

    tone_map_pass_setup_info.sampled_render_targets = {
        input_color_binding_desc,
        output_binding_desc
    };

    RendererInterface::BufferBindingDesc global_params_binding_desc{};
    global_params_binding_desc.binding_type = RendererInterface::BufferBindingDesc::CBV;
    global_params_binding_desc.buffer_handle = m_tone_map_global_params_handle;
    global_params_binding_desc.is_structured_buffer = false;
    tone_map_pass_setup_info.buffer_resources["ToneMapGlobalBuffer"] = global_params_binding_desc;

    const unsigned width = (std::max)(1u, resource_operator.GetCurrentRenderWidth());
    const unsigned height = (std::max)(1u, resource_operator.GetCurrentRenderHeight());
    RendererInterface::RenderExecuteCommand dispatch_command{};
    dispatch_command.type = RendererInterface::ExecuteCommandType::COMPUTE_DISPATCH_COMMAND;
    dispatch_command.parameter.dispatch_parameter.group_size_x = (width + 7) / 8;
    dispatch_command.parameter.dispatch_parameter.group_size_y = (height + 7) / 8;
    dispatch_command.parameter.dispatch_parameter.group_size_z = 1;
    tone_map_pass_setup_info.execute_command = dispatch_command;

    m_tone_map_pass_node = graph.CreateRenderGraphNode(resource_operator, tone_map_pass_setup_info);
    UploadGlobalParams(resource_operator);
    graph.RegisterRenderTargetToColorOutput(m_tone_map_output);
    return true;
}

bool RendererSystemToneMap::HasInit() const
{
    return m_tone_map_pass_node != NULL_HANDLE;
}

bool RendererSystemToneMap::Tick(RendererInterface::ResourceOperator& resource_operator,
                                 RendererInterface::RenderGraph& graph,
                                 unsigned long long interval)
{
    (void)interval;
    const unsigned width = (std::max)(1u, resource_operator.GetCurrentRenderWidth());
    const unsigned height = (std::max)(1u, resource_operator.GetCurrentRenderHeight());
    graph.UpdateComputeDispatch(m_tone_map_pass_node, (width + 7) / 8, (height + 7) / 8, 1);
    UploadGlobalParams(resource_operator);
    graph.RegisterRenderGraphNode(m_tone_map_pass_node);
    graph.RegisterRenderTargetToColorOutput(m_tone_map_output);
    return true;
}

void RendererSystemToneMap::OnResize(RendererInterface::ResourceOperator& resource_operator, unsigned width, unsigned height)
{
    (void)resource_operator;
    (void)width;
    (void)height;
}

void RendererSystemToneMap::DrawDebugUI()
{
    bool params_dirty = false;

    if (ImGui::SliderFloat("Exposure", &m_global_params.exposure, 0.1f, 4.0f, "%.2f"))
    {
        params_dirty = true;
    }

    if (ImGui::SliderFloat("Output Gamma", &m_global_params.gamma, 1.8f, 2.6f, "%.2f"))
    {
        params_dirty = true;
    }

    int tone_map_mode = static_cast<int>(m_global_params.tone_map_mode);
    const char* tone_map_modes[] = {"Reinhard", "ACES"};
    if (ImGui::Combo("ToneMap Mode", &tone_map_mode, tone_map_modes, IM_ARRAYSIZE(tone_map_modes)))
    {
        m_global_params.tone_map_mode = static_cast<unsigned>(tone_map_mode);
        params_dirty = true;
    }

    if (params_dirty)
    {
        const auto clamp = [](float value, float min_value, float max_value) -> float
        {
            return (std::max)(min_value, (std::min)(value, max_value));
        };

        m_global_params.exposure = clamp(m_global_params.exposure, 0.01f, 8.0f);
        m_global_params.gamma = clamp(m_global_params.gamma, 1.0f, 3.0f);
        m_need_upload_params = true;
    }
}

void RendererSystemToneMap::UploadGlobalParams(RendererInterface::ResourceOperator& resource_operator)
{
    if (!m_need_upload_params)
    {
        return;
    }

    RendererInterface::BufferUploadDesc global_params_upload_desc{};
    global_params_upload_desc.data = &m_global_params;
    global_params_upload_desc.size = sizeof(ToneMapGlobalParams);
    resource_operator.UploadBufferData(m_tone_map_global_params_handle, global_params_upload_desc);

    m_need_upload_params = false;
}
