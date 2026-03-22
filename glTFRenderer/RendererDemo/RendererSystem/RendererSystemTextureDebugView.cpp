#include "RendererSystemTextureDebugView.h"

#include "RendererSystemToneMap.h"

#include <algorithm>
#include <cmath>
#include <imgui/imgui.h>
#include <utility>

RendererSystemTextureDebugView::RendererSystemTextureDebugView(std::shared_ptr<RendererSystemToneMap> tone_map)
    : m_tone_map(std::move(tone_map))
{
}

bool RendererSystemTextureDebugView::RegisterSource(DebugSourceDesc source_desc)
{
    if (source_desc.id.empty() ||
        source_desc.display_name.empty() ||
        !source_desc.resolver ||
        FindSourceIndexById(source_desc.id) >= 0)
    {
        return false;
    }

    m_sources.push_back(std::move(source_desc));
    if (m_sources.size() == 1u && m_debug_state.source_id.empty())
    {
        return ApplySourceDefaults(0u);
    }
    return true;
}

bool RendererSystemTextureDebugView::SelectSourceById(const std::string& source_id, bool reset_to_defaults)
{
    const int source_index = FindSourceIndexById(source_id);
    if (source_index < 0)
    {
        return false;
    }

    if (reset_to_defaults)
    {
        return ApplySourceDefaults(static_cast<size_t>(source_index));
    }

    m_debug_state.source_id = source_id;
    ClampDebugState(m_debug_state);
    return true;
}

bool RendererSystemTextureDebugView::SetDebugState(const DebugState& state)
{
    if (FindSourceIndexById(state.source_id) < 0)
    {
        return false;
    }

    m_debug_state = state;
    ClampDebugState(m_debug_state);
    return true;
}

bool RendererSystemTextureDebugView::Init(RendererInterface::ResourceOperator& resource_operator,
                                          RendererInterface::RenderGraph& graph)
{
    GLTF_CHECK(m_tone_map);
    GLTF_CHECK(m_tone_map->HasInit());
    if (m_sources.empty())
    {
        return false;
    }

    m_output = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "TextureDebugView_Output",
        RendererInterface::RGBA8_UNORM,
        RendererInterface::default_clear_color,
        static_cast<RendererInterface::ResourceUsage>(
            RendererInterface::ResourceUsage::RENDER_TARGET |
            RendererInterface::ResourceUsage::COPY_SRC |
            RendererInterface::ResourceUsage::SHADER_RESOURCE |
            RendererInterface::ResourceUsage::UNORDER_ACCESS));

    RendererInterface::BufferDesc global_params_buffer_desc{};
    global_params_buffer_desc.name = "TextureDebugViewGlobalBuffer";
    global_params_buffer_desc.size = sizeof(TextureDebugViewGlobalParams);
    global_params_buffer_desc.type = RendererInterface::DEFAULT;
    global_params_buffer_desc.usage = RendererInterface::USAGE_CBV;
    m_global_params_handle = resource_operator.CreateBuffer(global_params_buffer_desc);

    const ExecutionPlan execution_plan = BuildExecutionPlan(resource_operator);
    RendererInterface::RenderGraphNodeHandle active_pass_node = NULL_HANDLE;
    RETURN_IF_FALSE(SyncActivePassNode(resource_operator, graph, execution_plan, active_pass_node));
    return true;
}

bool RendererSystemTextureDebugView::HasInit() const
{
    return m_output != NULL_HANDLE &&
           m_global_params_handle != NULL_HANDLE &&
           (m_color_pass_node != NULL_HANDLE || m_scalar_pass_node != NULL_HANDLE);
}

bool RendererSystemTextureDebugView::Tick(RendererInterface::ResourceOperator& resource_operator,
                                          RendererInterface::RenderGraph& graph,
                                          unsigned long long interval)
{
    (void)interval;
    const ExecutionPlan execution_plan = BuildExecutionPlan(resource_operator);
    RendererInterface::RenderGraphNodeHandle active_pass_node = NULL_HANDLE;
    RETURN_IF_FALSE(SyncActivePassNode(resource_operator, graph, execution_plan, active_pass_node));
    execution_plan.compute_plan.ApplyDispatch(graph, active_pass_node);
    UploadGlobalParams(resource_operator, execution_plan.source->visualization);
    RETURN_IF_FALSE(RenderFeature::RegisterRenderGraphNodeIfValid(graph, active_pass_node));
    graph.RegisterRenderTargetToColorOutput(m_output);
    return true;
}

void RendererSystemTextureDebugView::ResetRuntimeResources(RendererInterface::ResourceOperator& resource_operator)
{
    (void)resource_operator;
    m_color_pass_node = NULL_HANDLE;
    m_scalar_pass_node = NULL_HANDLE;
    m_output = NULL_HANDLE;
    m_global_params_handle = NULL_HANDLE;
    m_color_pass_cache.Reset();
    m_scalar_pass_cache.Reset();
}

void RendererSystemTextureDebugView::OnResize(RendererInterface::ResourceOperator& resource_operator,
                                              unsigned width,
                                              unsigned height)
{
    (void)resource_operator;
    (void)width;
    (void)height;
}

void RendererSystemTextureDebugView::DrawDebugUI()
{
    if (m_sources.empty())
    {
        ImGui::TextUnformatted("No registered debug sources.");
        return;
    }

    int current_index = FindSourceIndexById(m_debug_state.source_id);
    if (current_index < 0)
    {
        current_index = 0;
        ApplySourceDefaults(0u);
    }

    std::vector<const char*> source_labels{};
    source_labels.reserve(m_sources.size());
    for (const auto& source : m_sources)
    {
        source_labels.push_back(source.display_name.c_str());
    }

    if (ImGui::Combo("Source", &current_index, source_labels.data(), static_cast<int>(source_labels.size())))
    {
        ApplySourceDefaults(static_cast<size_t>(current_index));
    }

    const DebugSourceDesc* selected_source = GetSelectedSource();
    if (!selected_source)
    {
        return;
    }

    bool state_dirty = false;
    if (selected_source->visualization == VisualizationMode::Velocity)
    {
        if (ImGui::SliderFloat("Velocity Scale", &m_debug_state.scale, 1.0f, 256.0f, "%.1f"))
        {
            state_dirty = true;
        }
    }
    else
    {
        if (ImGui::DragFloat("Scale", &m_debug_state.scale, 0.02f, -64.0f, 64.0f, "%.3f"))
        {
            state_dirty = true;
        }
        if (ImGui::DragFloat("Bias", &m_debug_state.bias, 0.02f, -16.0f, 16.0f, "%.3f"))
        {
            state_dirty = true;
        }
    }

    if (selected_source->visualization == VisualizationMode::Color)
    {
        if (ImGui::Checkbox("Apply ToneMap", &m_debug_state.apply_tonemap))
        {
            state_dirty = true;
        }
    }

    if (state_dirty)
    {
        ClampDebugState(m_debug_state);
    }
}

bool RendererSystemTextureDebugView::IsScalarVisualization(VisualizationMode visualization)
{
    return visualization == VisualizationMode::Scalar;
}

void RendererSystemTextureDebugView::ClampDebugState(DebugState& state)
{
    if (!std::isfinite(state.scale))
    {
        state.scale = 1.0f;
    }
    if (!std::isfinite(state.bias))
    {
        state.bias = 0.0f;
    }

    state.scale = std::clamp(state.scale, -512.0f, 512.0f);
    state.bias = std::clamp(state.bias, -32.0f, 32.0f);
}

int RendererSystemTextureDebugView::FindSourceIndexById(const std::string& source_id) const
{
    const auto it = std::find_if(
        m_sources.begin(),
        m_sources.end(),
        [&](const DebugSourceDesc& source)
        {
            return source.id == source_id;
        });
    if (it == m_sources.end())
    {
        return -1;
    }
    return static_cast<int>(std::distance(m_sources.begin(), it));
}

bool RendererSystemTextureDebugView::ApplySourceDefaults(size_t source_index)
{
    if (source_index >= m_sources.size())
    {
        return false;
    }

    const DebugSourceDesc& source = m_sources[source_index];
    m_debug_state.source_id = source.id;
    m_debug_state.scale = source.default_scale;
    m_debug_state.bias = source.default_bias;
    m_debug_state.apply_tonemap = source.default_apply_tonemap;
    ClampDebugState(m_debug_state);
    return true;
}

const RendererSystemTextureDebugView::DebugSourceDesc* RendererSystemTextureDebugView::GetSelectedSource() const
{
    const int source_index = FindSourceIndexById(m_debug_state.source_id);
    if (source_index < 0)
    {
        return nullptr;
    }
    return &m_sources[static_cast<size_t>(source_index)];
}

RendererSystemTextureDebugView::ExecutionPlan RendererSystemTextureDebugView::BuildExecutionPlan(
    RendererInterface::ResourceOperator& resource_operator) const
{
    const DebugSourceDesc* selected_source = GetSelectedSource();
    GLTF_CHECK(selected_source);
    const SourceResolution resolved_source = selected_source->resolver();
    GLTF_CHECK(resolved_source.render_target != NULL_HANDLE);

    return ExecutionPlan{
        .source = selected_source,
        .resolved_source = resolved_source,
        .compute_plan = RenderFeature::ComputeExecutionPlan::FromResourceOperator(resource_operator)
    };
}

RendererInterface::RenderGraph::RenderPassSetupInfo RendererSystemTextureDebugView::BuildColorPassSetupInfo(
    const ExecutionPlan& execution_plan) const
{
    return RenderFeature::PassBuilder::Compute("Texture Debug View", "Texture Debug Color")
        .AddShader(RendererInterface::COMPUTE_SHADER, "MainColor", "Resources/Shaders/TextureDebugView.hlsl")
        .AddSampledRenderTargetBindings({
            RenderFeature::MakeSampledRenderTargetBinding(
                "InputColorTex",
                execution_plan.resolved_source.render_target,
                RendererInterface::RenderTargetTextureBindingDesc::SRV),
            RenderFeature::MakeSampledRenderTargetBinding(
                "Output",
                m_output,
                RendererInterface::RenderTargetTextureBindingDesc::UAV)
        })
        .AddBuffers({
            RenderFeature::MakeBufferBinding(
                "TextureDebugViewGlobalBuffer",
                RenderFeature::MakeConstantBufferBinding(m_global_params_handle))
        })
        .AddDependency(execution_plan.resolved_source.dependency_node)
        .SetDispatch(execution_plan.compute_plan)
        .Build();
}

RendererInterface::RenderGraph::RenderPassSetupInfo RendererSystemTextureDebugView::BuildScalarPassSetupInfo(
    const ExecutionPlan& execution_plan) const
{
    return RenderFeature::PassBuilder::Compute("Texture Debug View", "Texture Debug Scalar")
        .AddShader(RendererInterface::COMPUTE_SHADER, "MainScalar", "Resources/Shaders/TextureDebugView.hlsl")
        .AddSampledRenderTargetBindings({
            RenderFeature::MakeSampledRenderTargetBinding(
                "InputScalarTex",
                execution_plan.resolved_source.render_target,
                RendererInterface::RenderTargetTextureBindingDesc::SRV),
            RenderFeature::MakeSampledRenderTargetBinding(
                "Output",
                m_output,
                RendererInterface::RenderTargetTextureBindingDesc::UAV)
        })
        .AddBuffers({
            RenderFeature::MakeBufferBinding(
                "TextureDebugViewGlobalBuffer",
                RenderFeature::MakeConstantBufferBinding(m_global_params_handle))
        })
        .AddDependency(execution_plan.resolved_source.dependency_node)
        .SetDispatch(execution_plan.compute_plan)
        .Build();
}

bool RendererSystemTextureDebugView::SyncActivePassNode(RendererInterface::ResourceOperator& resource_operator,
                                                        RendererInterface::RenderGraph& graph,
                                                        const ExecutionPlan& execution_plan,
                                                        RendererInterface::RenderGraphNodeHandle& out_active_node_handle)
{
    if (IsScalarVisualization(execution_plan.source->visualization))
    {
        const bool need_sync =
            !m_scalar_pass_cache.valid ||
            m_scalar_pass_cache.source_render_target != execution_plan.resolved_source.render_target ||
            m_scalar_pass_cache.dependency_node != execution_plan.resolved_source.dependency_node;
        out_active_node_handle = m_scalar_pass_node;
        RETURN_IF_FALSE(RenderFeature::CreateOrSyncRenderGraphNode(
            need_sync,
            resource_operator,
            graph,
            out_active_node_handle,
            BuildScalarPassSetupInfo(execution_plan)));
        m_scalar_pass_node = out_active_node_handle;
        m_scalar_pass_cache.valid = true;
        m_scalar_pass_cache.source_render_target = execution_plan.resolved_source.render_target;
        m_scalar_pass_cache.dependency_node = execution_plan.resolved_source.dependency_node;
        return true;
    }

    const bool need_sync =
        !m_color_pass_cache.valid ||
        m_color_pass_cache.source_render_target != execution_plan.resolved_source.render_target ||
        m_color_pass_cache.dependency_node != execution_plan.resolved_source.dependency_node;
    out_active_node_handle = m_color_pass_node;
    RETURN_IF_FALSE(RenderFeature::CreateOrSyncRenderGraphNode(
        need_sync,
        resource_operator,
        graph,
        out_active_node_handle,
        BuildColorPassSetupInfo(execution_plan)));
    m_color_pass_node = out_active_node_handle;
    m_color_pass_cache.valid = true;
    m_color_pass_cache.source_render_target = execution_plan.resolved_source.render_target;
    m_color_pass_cache.dependency_node = execution_plan.resolved_source.dependency_node;
    return true;
}

void RendererSystemTextureDebugView::UploadGlobalParams(RendererInterface::ResourceOperator& resource_operator,
                                                        VisualizationMode visualization)
{
    TextureDebugViewGlobalParams global_params{};
    global_params.visualization_mode = static_cast<unsigned>(visualization);
    global_params.scale = m_debug_state.scale;
    global_params.bias = m_debug_state.bias;
    if (m_tone_map)
    {
        const auto& tone_map_params = m_tone_map->GetGlobalParams();
        global_params.exposure = tone_map_params.exposure;
        global_params.gamma = tone_map_params.gamma;
        global_params.tone_map_mode = tone_map_params.tone_map_mode;
    }
    global_params.apply_tonemap =
        (visualization == VisualizationMode::Color && m_debug_state.apply_tonemap) ? 1u : 0u;

    RendererInterface::BufferUploadDesc global_params_upload_desc{};
    global_params_upload_desc.data = &global_params;
    global_params_upload_desc.size = sizeof(TextureDebugViewGlobalParams);
    resource_operator.UploadBufferData(m_global_params_handle, global_params_upload_desc);
}
