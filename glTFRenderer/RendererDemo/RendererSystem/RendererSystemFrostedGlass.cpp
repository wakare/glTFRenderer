#include "RendererSystemFrostedGlass.h"
#include <algorithm>
#include <imgui/imgui.h>

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
    m_need_upload_panels = true;
    return index;
}

bool RendererSystemFrostedGlass::UpdatePanel(unsigned index, const FrostedGlassPanelDesc& panel_desc)
{
    GLTF_CHECK(ContainsPanel(index));
    m_panel_descs[index] = panel_desc;
    m_need_upload_panels = true;
    return true;
}

bool RendererSystemFrostedGlass::ContainsPanel(unsigned index) const
{
    return index < m_panel_descs.size();
}

bool RendererSystemFrostedGlass::Init(RendererInterface::ResourceOperator& resource_operator,
                                      RendererInterface::RenderGraph& graph)
{
    GLTF_CHECK(m_scene);
    GLTF_CHECK(m_lighting);
    GLTF_CHECK(m_scene->HasInit());
    GLTF_CHECK(m_lighting->HasInit());

    const unsigned width = m_scene->GetWidth();
    const unsigned height = m_scene->GetHeight();
    m_frosted_pass_output = resource_operator.CreateRenderTarget(
        "FrostedGlass_Output",
        width,
        height,
        RendererInterface::RGBA8_UNORM,
        RendererInterface::default_clear_color,
        static_cast<RendererInterface::ResourceUsage>(
            RendererInterface::ResourceUsage::RENDER_TARGET |
            RendererInterface::ResourceUsage::COPY_SRC |
            RendererInterface::ResourceUsage::SHADER_RESOURCE |
            RendererInterface::ResourceUsage::UNORDER_ACCESS));

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

    RendererInterface::RenderGraph::RenderPassSetupInfo frosted_pass_setup_info{};
    frosted_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    frosted_pass_setup_info.debug_group = "Frosted Glass";
    frosted_pass_setup_info.debug_name = "Frosted Composite";
    frosted_pass_setup_info.modules = {m_scene->GetCameraModule()};
    frosted_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "main", "Resources/Shaders/FrostedGlass.hlsl"}
    };

    RendererSystemOutput<RendererSystemSceneRenderer> scene_output;

    RendererInterface::RenderTargetTextureBindingDesc input_color_binding_desc{};
    input_color_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
    input_color_binding_desc.name = "InputColorTex";
    input_color_binding_desc.render_target_texture = {m_lighting->GetLightingOutput()};

    RendererInterface::RenderTargetTextureBindingDesc input_depth_binding_desc{};
    input_depth_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
    input_depth_binding_desc.name = "InputDepthTex";
    input_depth_binding_desc.render_target_texture = {scene_output.GetRenderTargetHandle(*m_scene, "m_base_pass_depth")};

    RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
    output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
    output_binding_desc.name = "Output";
    output_binding_desc.render_target_texture = {m_frosted_pass_output};

    frosted_pass_setup_info.sampled_render_targets = {
        input_color_binding_desc,
        input_depth_binding_desc,
        output_binding_desc
    };

    RendererInterface::BufferBindingDesc panel_data_binding_desc{};
    panel_data_binding_desc.binding_type = RendererInterface::BufferBindingDesc::SRV;
    panel_data_binding_desc.buffer_handle = m_frosted_panel_data_handle;
    panel_data_binding_desc.stride = sizeof(FrostedGlassPanelGpuData);
    panel_data_binding_desc.is_structured_buffer = true;
    panel_data_binding_desc.count = MAX_PANEL_COUNT;
    frosted_pass_setup_info.buffer_resources["g_frosted_panels"] = panel_data_binding_desc;

    RendererInterface::BufferBindingDesc global_params_binding_desc{};
    global_params_binding_desc.binding_type = RendererInterface::BufferBindingDesc::CBV;
    global_params_binding_desc.buffer_handle = m_frosted_global_params_handle;
    global_params_binding_desc.is_structured_buffer = false;
    frosted_pass_setup_info.buffer_resources["FrostedGlassGlobalBuffer"] = global_params_binding_desc;

    RendererInterface::RenderExecuteCommand dispatch_command{};
    dispatch_command.type = RendererInterface::ExecuteCommandType::COMPUTE_DISPATCH_COMMAND;
    dispatch_command.parameter.dispatch_parameter.group_size_x = (width + 7) / 8;
    dispatch_command.parameter.dispatch_parameter.group_size_y = (height + 7) / 8;
    dispatch_command.parameter.dispatch_parameter.group_size_z = 1;
    frosted_pass_setup_info.execute_command = dispatch_command;

    m_frosted_pass_node = graph.CreateRenderGraphNode(resource_operator, frosted_pass_setup_info);
    UploadPanelData(resource_operator);
    graph.RegisterRenderTargetToColorOutput(m_frosted_pass_output);
    return true;
}

bool RendererSystemFrostedGlass::HasInit() const
{
    return m_frosted_pass_node != NULL_HANDLE;
}

bool RendererSystemFrostedGlass::Tick(RendererInterface::ResourceOperator& resource_operator,
                                      RendererInterface::RenderGraph& graph,
                                      unsigned long long interval)
{
    (void)interval;
    const unsigned width = m_scene->GetWidth();
    const unsigned height = m_scene->GetHeight();
    graph.UpdateComputeDispatch(m_frosted_pass_node, (width + 7) / 8, (height + 7) / 8, 1);
    UploadPanelData(resource_operator);
    graph.RegisterRenderGraphNode(m_frosted_pass_node);
    return true;
}

void RendererSystemFrostedGlass::OnResize(RendererInterface::ResourceOperator& resource_operator, unsigned width, unsigned height)
{
    (void)resource_operator;
    (void)width;
    (void)height;
}

void RendererSystemFrostedGlass::DrawDebugUI()
{
    bool panel_dirty = false;

    int blur_radius = static_cast<int>(m_global_params.blur_radius);
    if (ImGui::SliderInt("Blur Radius", &blur_radius, 1, 12))
    {
        m_global_params.blur_radius = static_cast<unsigned>(blur_radius);
        panel_dirty = true;
    }
    if (ImGui::SliderFloat("Scene Edge Scale", &m_global_params.scene_edge_scale, 0.0f, 120.0f, "%.1f"))
    {
        panel_dirty = true;
    }

    if (m_panel_descs.empty())
    {
        ImGui::TextUnformatted("No frosted panels.");
        if (panel_dirty)
        {
            m_need_upload_panels = true;
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
    const char* shape_types[] = {"RoundedRect", "Circle", "ShapeMask(Reserved)"};
    if (ImGui::Combo("Shape Type", &shape_type, shape_types, IM_ARRAYSIZE(shape_types)))
    {
        panel.shape_type = static_cast<PanelShapeType>(shape_type);
        panel_dirty = true;
    }

    const float max_corner_radius = (std::max)(0.0f, (std::min)(panel.half_size_uv.x, panel.half_size_uv.y) - 0.001f);
    if (ImGui::SliderFloat("Corner Radius", &panel.corner_radius, 0.0f, max_corner_radius, "%.3f"))
    {
        panel_dirty = true;
    }
    if (ImGui::SliderFloat("Blur Sigma", &panel.blur_sigma, 0.2f, 12.0f, "%.2f"))
    {
        panel_dirty = true;
    }
    if (ImGui::SliderFloat("Blur Strength", &panel.blur_strength, 0.0f, 1.0f, "%.2f"))
    {
        panel_dirty = true;
    }
    if (ImGui::SliderFloat("Rim Intensity", &panel.rim_intensity, 0.0f, 0.6f, "%.3f"))
    {
        panel_dirty = true;
    }
    if (ImGui::ColorEdit3("Tint Color", &panel.tint_color.x))
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
        panel.blur_sigma = clamp(panel.blur_sigma, 0.2f, 12.0f);
        panel.blur_strength = clamp(panel.blur_strength, 0.0f, 1.0f);
        panel.rim_intensity = clamp(panel.rim_intensity, 0.0f, 1.0f);
        panel.edge_softness = clamp(panel.edge_softness, 0.1f, 8.0f);
        m_need_upload_panels = true;
    }
}

void RendererSystemFrostedGlass::UploadPanelData(RendererInterface::ResourceOperator& resource_operator)
{
    if (!m_need_upload_panels)
    {
        return;
    }

    std::vector<FrostedGlassPanelGpuData> panel_gpu_datas;
    panel_gpu_datas.reserve(m_panel_descs.size());
    for (const auto& panel_desc : m_panel_descs)
    {
        panel_gpu_datas.push_back(ConvertPanelToGpuData(panel_desc));
    }

    if (!panel_gpu_datas.empty())
    {
        RendererInterface::BufferUploadDesc panel_data_upload_desc{};
        panel_data_upload_desc.data = panel_gpu_datas.data();
        panel_data_upload_desc.size = panel_gpu_datas.size() * sizeof(FrostedGlassPanelGpuData);
        resource_operator.UploadBufferData(m_frosted_panel_data_handle, panel_data_upload_desc);
    }

    m_global_params.panel_count = static_cast<unsigned>(panel_gpu_datas.size());
    RendererInterface::BufferUploadDesc global_params_upload_desc{};
    global_params_upload_desc.data = &m_global_params;
    global_params_upload_desc.size = sizeof(FrostedGlassGlobalParams);
    resource_operator.UploadBufferData(m_frosted_global_params_handle, global_params_upload_desc);

    m_need_upload_panels = false;
}

RendererSystemFrostedGlass::FrostedGlassPanelGpuData RendererSystemFrostedGlass::ConvertPanelToGpuData(
    const FrostedGlassPanelDesc& panel_desc) const
{
    FrostedGlassPanelGpuData gpu_data{};
    gpu_data.center_half_size = {
        panel_desc.center_uv.x,
        panel_desc.center_uv.y,
        panel_desc.half_size_uv.x,
        panel_desc.half_size_uv.y
    };
    gpu_data.corner_blur_rim = {
        panel_desc.corner_radius,
        panel_desc.blur_sigma,
        panel_desc.blur_strength,
        panel_desc.rim_intensity
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
        panel_desc.pad0
    };
    return gpu_data;
}
