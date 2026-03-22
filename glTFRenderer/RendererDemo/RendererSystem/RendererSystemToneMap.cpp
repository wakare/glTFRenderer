#include "RendererSystemToneMap.h"

#include "RenderPassSetupBuilder.h"
#include "RendererSystemFrostedGlass.h"
#include "RendererSystemLighting.h"
#include <algorithm>
#include <imgui/imgui.h>
#include <utility>

RendererSystemToneMap::RendererSystemToneMap(std::shared_ptr<RendererSystemFrostedGlass> frosted,
                                             std::shared_ptr<RendererSystemLighting> lighting)
    : m_frosted(std::move(frosted))
    , m_lighting(std::move(lighting))
{
}

void RendererSystemToneMap::SetGlobalParams(const ToneMapGlobalParams& global_params)
{
    m_global_params = global_params;
    ClampGlobalParams(m_global_params);
    m_need_upload_params = true;
}

bool RendererSystemToneMap::Init(RendererInterface::ResourceOperator& resource_operator,
                                 RendererInterface::RenderGraph& graph)
{
    GLTF_CHECK(m_lighting);
    GLTF_CHECK(m_lighting->HasInit());
    if (m_frosted)
    {
        GLTF_CHECK(m_frosted->HasInit());
    }

    m_tone_map_output = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
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

    const ToneMapExecutionPlan execution_plan = BuildToneMapExecutionPlan(resource_operator);
    RETURN_IF_FALSE(RenderFeature::CreateRenderGraphNodeIfNeeded(
        resource_operator,
        graph,
        m_tone_map_pass_node,
        BuildToneMapPassSetupInfo(execution_plan)));
    UploadGlobalParams(resource_operator);
    return true;
}

RendererInterface::RenderGraph::RenderPassSetupInfo RendererSystemToneMap::BuildToneMapPassSetupInfo(
    const ToneMapExecutionPlan& execution_plan) const
{
    return RenderFeature::PassBuilder::Compute("Tone Map", "Tone Map Composite")
        .AddShader(RendererInterface::COMPUTE_SHADER, "main", "Resources/Shaders/ToneMap.hlsl")
        .AddSampledRenderTargetBindings({
            RenderFeature::MakeSampledRenderTargetBinding(
                "InputColorTex",
                execution_plan.input_color,
                RendererInterface::RenderTargetTextureBindingDesc::SRV),
            RenderFeature::MakeSampledRenderTargetBinding(
                "Output",
                m_tone_map_output,
                RendererInterface::RenderTargetTextureBindingDesc::UAV)
        })
        .AddBuffers({
            RenderFeature::MakeBufferBinding(
                "ToneMapGlobalBuffer",
                RenderFeature::MakeConstantBufferBinding(m_tone_map_global_params_handle))
        })
        .SetDispatch(execution_plan.compute_plan)
        .Build();
}

RendererSystemToneMap::ToneMapExecutionPlan RendererSystemToneMap::BuildToneMapExecutionPlan(
    RendererInterface::ResourceOperator& resource_operator) const
{
    const auto lighting_outputs = m_lighting->GetOutputs();
    const RendererInterface::RenderTargetHandle input_color = m_frosted
        ? m_frosted->GetOutput()
        : lighting_outputs.output;
    GLTF_CHECK(input_color != NULL_HANDLE);
    return ToneMapExecutionPlan{
        .input_color = input_color,
        .compute_plan = RenderFeature::ComputeExecutionPlan::FromResourceOperator(resource_operator)
    };
}

bool RendererSystemToneMap::HasInit() const
{
    return m_tone_map_pass_node != NULL_HANDLE;
}

void RendererSystemToneMap::ResetRuntimeResources(RendererInterface::ResourceOperator& resource_operator)
{
    (void)resource_operator;
    m_tone_map_pass_node = NULL_HANDLE;
    m_tone_map_output = NULL_HANDLE;
    m_tone_map_global_params_handle = NULL_HANDLE;
    m_need_upload_params = true;
}

bool RendererSystemToneMap::Tick(RendererInterface::ResourceOperator& resource_operator,
                                 RendererInterface::RenderGraph& graph,
                                 unsigned long long interval)
{
    (void)interval;
    const ToneMapExecutionPlan execution_plan = BuildToneMapExecutionPlan(resource_operator);
    execution_plan.compute_plan.ApplyDispatch(graph, m_tone_map_pass_node);
    UploadGlobalParams(resource_operator);
    RETURN_IF_FALSE(RenderFeature::RegisterRenderGraphNodeIfValid(graph, m_tone_map_pass_node));
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
        ClampGlobalParams(m_global_params);
        m_need_upload_params = true;
    }
}

void RendererSystemToneMap::ClampGlobalParams(ToneMapGlobalParams& global_params)
{
    global_params.exposure = std::clamp(global_params.exposure, 0.01f, 8.0f);
    global_params.gamma = std::clamp(global_params.gamma, 1.0f, 3.0f);
    global_params.tone_map_mode = (std::min)(global_params.tone_map_mode, 1u);
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
