#include "RendererSystemSSAO.h"

#include "RendererSystemSceneRenderer.h"

#include <algorithm>
#include <imgui/imgui.h>
#include <utility>

RendererSystemSSAO::RendererSystemSSAO(std::shared_ptr<RendererSystemSceneRenderer> scene)
    : m_scene(std::move(scene))
{
}

RendererInterface::RenderTargetHandle RendererSystemSSAO::GetOutput() const
{
    return GetOutputs().output;
}

RendererInterface::RenderGraphNodeHandle RendererSystemSSAO::GetOutputNode() const
{
    return GetOutputs().blur_node;
}

RendererSystemSSAO::SSAOOutputs RendererSystemSSAO::GetOutputs() const
{
    const bool run_blur_passes = ShouldRunBlurPasses();
    return SSAOOutputs{
        .raw_node = m_ssao_pass_node,
        .blur_node = run_blur_passes ? m_blur_pass_node : m_ssao_pass_node,
        .raw_output = m_ssao_raw_output,
        .output = run_blur_passes ? m_ssao_output : m_ssao_raw_output
    };
}

const RendererSystemSSAO::SSAOGlobalParams& RendererSystemSSAO::GetGlobalParams() const
{
    return m_global_params;
}

void RendererSystemSSAO::SetGlobalParams(const SSAOGlobalParams& global_params)
{
    SSAOGlobalParams clamped_params = global_params;
    ClampGlobalParams(clamped_params);
    m_global_params = clamped_params;
    m_need_upload_params = true;
}

bool RendererSystemSSAO::Init(RendererInterface::ResourceOperator& resource_operator,
                              RendererInterface::RenderGraph& graph)
{
    GLTF_CHECK(m_scene);
    GLTF_CHECK(m_scene->HasInit());

    CreateOutputs(resource_operator);

    RendererInterface::BufferDesc global_params_buffer_desc{};
    global_params_buffer_desc.name = "SSAOGlobalBuffer";
    global_params_buffer_desc.size = sizeof(SSAOGlobalParams);
    global_params_buffer_desc.type = RendererInterface::DEFAULT;
    global_params_buffer_desc.usage = RendererInterface::USAGE_CBV;
    m_ssao_global_params_handle = resource_operator.CreateBuffer(global_params_buffer_desc);

    const SSAOExecutionPlan execution_plan = BuildExecutionPlan();
    RETURN_IF_FALSE(RenderFeature::CreateRenderGraphNodeIfNeeded(
        resource_operator,
        graph,
        m_ssao_pass_node,
        BuildSSAOPassSetupInfo(execution_plan)));
    RETURN_IF_FALSE(RenderFeature::CreateRenderGraphNodeIfNeeded(
        resource_operator,
        graph,
        m_blur_horizontal_pass_node,
        BuildBlurHorizontalPassSetupInfo(execution_plan)));
    RETURN_IF_FALSE(RenderFeature::CreateRenderGraphNodeIfNeeded(
        resource_operator,
        graph,
        m_blur_pass_node,
        BuildBlurVerticalPassSetupInfo(execution_plan)));

    UploadGlobalParams(resource_operator);
    return true;
}

bool RendererSystemSSAO::HasInit() const
{
    return m_blur_horizontal_pass_node != NULL_HANDLE &&
        m_blur_pass_node != NULL_HANDLE &&
        m_ssao_blur_temp_output != NULL_HANDLE &&
        m_ssao_output != NULL_HANDLE;
}

bool RendererSystemSSAO::Tick(RendererInterface::ResourceOperator& resource_operator,
                              RendererInterface::RenderGraph& graph,
                              unsigned long long interval)
{
    (void)interval;

    const SSAOExecutionPlan execution_plan = BuildExecutionPlan();
    const bool run_blur_passes = ShouldRunBlurPasses();
    execution_plan.compute_plan.ApplyDispatch(graph, m_ssao_pass_node);
    if (run_blur_passes)
    {
        execution_plan.compute_plan.ApplyDispatch(graph, m_blur_horizontal_pass_node);
        execution_plan.compute_plan.ApplyDispatch(graph, m_blur_pass_node);
    }
    UploadGlobalParams(resource_operator);

    if (run_blur_passes)
    {
        RETURN_IF_FALSE(RenderFeature::RegisterRenderGraphNodesIfValid(
            graph,
            {m_ssao_pass_node, m_blur_horizontal_pass_node, m_blur_pass_node}));
    }
    else
    {
        RETURN_IF_FALSE(RenderFeature::RegisterRenderGraphNodeIfValid(graph, m_ssao_pass_node));
    }
    return true;
}

void RendererSystemSSAO::ResetRuntimeResources(RendererInterface::ResourceOperator& resource_operator)
{
    (void)resource_operator;
    m_ssao_pass_node = NULL_HANDLE;
    m_blur_horizontal_pass_node = NULL_HANDLE;
    m_blur_pass_node = NULL_HANDLE;
    m_ssao_raw_output = NULL_HANDLE;
    m_ssao_blur_temp_output = NULL_HANDLE;
    m_ssao_output = NULL_HANDLE;
    m_ssao_global_params_handle = NULL_HANDLE;
    m_need_upload_params = true;
}

void RendererSystemSSAO::OnResize(RendererInterface::ResourceOperator& resource_operator, unsigned width, unsigned height)
{
    (void)resource_operator;
    (void)width;
    (void)height;
}

void RendererSystemSSAO::DrawDebugUI()
{
    bool params_dirty = false;
    const char* debug_output_modes[] = {"AO", "Valid Sample Ratio"};

    bool enabled = m_global_params.enabled != 0u;
    if (ImGui::Checkbox("Enable SSAO", &enabled))
    {
        m_global_params.enabled = enabled ? 1u : 0u;
        params_dirty = true;
    }

    params_dirty |= ImGui::SliderFloat("AO Radius", &m_global_params.radius_world, 0.05f, 2.5f, "%.2f");
    params_dirty |= ImGui::SliderFloat("AO Strength", &m_global_params.intensity, 0.0f, 4.0f, "%.2f");
    params_dirty |= ImGui::SliderFloat("AO Power", &m_global_params.power, 0.5f, 4.0f, "%.2f");
    params_dirty |= ImGui::SliderFloat("AO Bias", &m_global_params.bias, 0.001f, 0.15f, "%.3f");
    params_dirty |= ImGui::SliderFloat("AO Thickness", &m_global_params.thickness, 0.02f, 1.0f, "%.2f");
    params_dirty |= ImGui::SliderFloat("Sample Distribution", &m_global_params.sample_distribution_power, 0.5f, 4.0f, "%.2f");
    params_dirty |= ImGui::SliderFloat("Blur Depth Reject", &m_global_params.blur_depth_reject, 0.5f, 32.0f, "%.2f");
    params_dirty |= ImGui::SliderFloat("Blur Normal Reject", &m_global_params.blur_normal_reject, 1.0f, 96.0f, "%.2f");

    int sample_count = static_cast<int>(m_global_params.sample_count);
    if (ImGui::SliderInt("Sample Count", &sample_count, 4, 32))
    {
        m_global_params.sample_count = static_cast<unsigned>(sample_count);
        params_dirty = true;
    }

    int blur_radius = static_cast<int>(m_global_params.blur_radius);
    if (ImGui::SliderInt("Blur Radius", &blur_radius, 0, 3))
    {
        m_global_params.blur_radius = static_cast<unsigned>(blur_radius);
        params_dirty = true;
    }

    int debug_output_mode = static_cast<int>(m_global_params.debug_output_mode);
    if (ImGui::Combo("Debug Output", &debug_output_mode, debug_output_modes, IM_ARRAYSIZE(debug_output_modes)))
    {
        m_global_params.debug_output_mode = static_cast<unsigned>(debug_output_mode);
        params_dirty = true;
    }

    if (m_global_params.debug_output_mode != 0u)
    {
        ImGui::TextUnformatted("Debug output overrides the SSAO signal used by lighting.");
        ImGui::TextUnformatted("Inspect it with Tone Map > Debug View = SSAO.");
    }

    if (params_dirty)
    {
        ClampGlobalParams(m_global_params);
        m_need_upload_params = true;
    }
}

void RendererSystemSSAO::CreateOutputs(RendererInterface::ResourceOperator& resource_operator)
{
    RendererInterface::RenderTargetClearValue white_clear = RendererInterface::default_clear_color;
    white_clear.clear_color[0] = 1.0f;
    white_clear.clear_color[1] = 1.0f;
    white_clear.clear_color[2] = 1.0f;
    white_clear.clear_color[3] = 1.0f;

    const auto usage = static_cast<RendererInterface::ResourceUsage>(
        RendererInterface::ResourceUsage::COPY_SRC |
        RendererInterface::ResourceUsage::SHADER_RESOURCE |
        RendererInterface::ResourceUsage::UNORDER_ACCESS);

    m_ssao_raw_output = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "SSAO_Raw",
        RendererInterface::R16_FLOAT,
        white_clear,
        usage);
    m_ssao_blur_temp_output = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "SSAO_BlurTemp",
        RendererInterface::R16_FLOAT,
        white_clear,
        usage);
    m_ssao_output = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "SSAO_Output",
        RendererInterface::R16_FLOAT,
        white_clear,
        usage);
}

RendererSystemSSAO::SSAOExecutionPlan RendererSystemSSAO::BuildExecutionPlan() const
{
    const auto camera_module = m_scene->GetCameraModule();
    GLTF_CHECK(camera_module);
    const unsigned extent_width = m_scene->GetWidth() > 0
        ? m_scene->GetWidth()
        : camera_module->GetWidth();
    const unsigned extent_height = m_scene->GetHeight() > 0
        ? m_scene->GetHeight()
        : camera_module->GetHeight();

    return SSAOExecutionPlan{
        .camera_module = camera_module,
        .compute_plan = RenderFeature::ComputeExecutionPlan::FromExtent(extent_width, extent_height)
    };
}

RendererInterface::RenderGraph::RenderPassSetupInfo RendererSystemSSAO::BuildSSAOPassSetupInfo(
    const SSAOExecutionPlan& execution_plan) const
{
    const auto scene_outputs = m_scene->GetOutputs();
    return RenderFeature::PassBuilder::Compute("SSAO", "SSAO Evaluate")
        .AddModule(execution_plan.camera_module)
        .AddDependency(scene_outputs.node)
        .AddShader(RendererInterface::COMPUTE_SHADER, "MainSSAO", "Resources/Shaders/SSAO.hlsl")
        .AddSampledRenderTargetBindings({
            RenderFeature::MakeSampledRenderTargetBinding(
                "normalTex",
                scene_outputs.normal,
                RendererInterface::RenderTargetTextureBindingDesc::SRV),
            RenderFeature::MakeSampledRenderTargetBinding(
                "depthTex",
                scene_outputs.depth,
                RendererInterface::RenderTargetTextureBindingDesc::SRV),
            RenderFeature::MakeSampledRenderTargetBinding(
                "Output",
                m_ssao_raw_output,
                RendererInterface::RenderTargetTextureBindingDesc::UAV)
        })
        .AddBuffers({
            RenderFeature::MakeBufferBinding(
                "SSAOGlobalBuffer",
                RenderFeature::MakeConstantBufferBinding(m_ssao_global_params_handle))
        })
        .SetDispatch(execution_plan.compute_plan)
        .Build();
}

RendererInterface::RenderGraph::RenderPassSetupInfo RendererSystemSSAO::BuildBlurHorizontalPassSetupInfo(
    const SSAOExecutionPlan& execution_plan) const
{
    const auto scene_outputs = m_scene->GetOutputs();
    return RenderFeature::PassBuilder::Compute("SSAO", "SSAO Blur X")
        .AddModule(execution_plan.camera_module)
        .AddDependency(m_ssao_pass_node)
        .AddShader(RendererInterface::COMPUTE_SHADER, "MainSSAOBlurHorizontal", "Resources/Shaders/SSAO.hlsl")
        .AddSampledRenderTargetBindings({
            RenderFeature::MakeSampledRenderTargetBinding(
                "InputAOTex",
                m_ssao_raw_output,
                RendererInterface::RenderTargetTextureBindingDesc::SRV),
            RenderFeature::MakeSampledRenderTargetBinding(
                "normalTex",
                scene_outputs.normal,
                RendererInterface::RenderTargetTextureBindingDesc::SRV),
            RenderFeature::MakeSampledRenderTargetBinding(
                "depthTex",
                scene_outputs.depth,
                RendererInterface::RenderTargetTextureBindingDesc::SRV),
            RenderFeature::MakeSampledRenderTargetBinding(
                "Output",
                m_ssao_blur_temp_output,
                RendererInterface::RenderTargetTextureBindingDesc::UAV)
        })
        .AddBuffers({
            RenderFeature::MakeBufferBinding(
                "SSAOGlobalBuffer",
                RenderFeature::MakeConstantBufferBinding(m_ssao_global_params_handle))
        })
        .SetDispatch(execution_plan.compute_plan)
        .Build();
}

RendererInterface::RenderGraph::RenderPassSetupInfo RendererSystemSSAO::BuildBlurVerticalPassSetupInfo(
    const SSAOExecutionPlan& execution_plan) const
{
    const auto scene_outputs = m_scene->GetOutputs();
    return RenderFeature::PassBuilder::Compute("SSAO", "SSAO Blur Y")
        .AddModule(execution_plan.camera_module)
        .AddDependency(m_blur_horizontal_pass_node)
        .AddShader(RendererInterface::COMPUTE_SHADER, "MainSSAOBlurVertical", "Resources/Shaders/SSAO.hlsl")
        .AddSampledRenderTargetBindings({
            RenderFeature::MakeSampledRenderTargetBinding(
                "InputAOTex",
                m_ssao_blur_temp_output,
                RendererInterface::RenderTargetTextureBindingDesc::SRV),
            RenderFeature::MakeSampledRenderTargetBinding(
                "normalTex",
                scene_outputs.normal,
                RendererInterface::RenderTargetTextureBindingDesc::SRV),
            RenderFeature::MakeSampledRenderTargetBinding(
                "depthTex",
                scene_outputs.depth,
                RendererInterface::RenderTargetTextureBindingDesc::SRV),
            RenderFeature::MakeSampledRenderTargetBinding(
                "Output",
                m_ssao_output,
                RendererInterface::RenderTargetTextureBindingDesc::UAV)
        })
        .AddBuffers({
            RenderFeature::MakeBufferBinding(
                "SSAOGlobalBuffer",
                RenderFeature::MakeConstantBufferBinding(m_ssao_global_params_handle))
        })
        .SetDispatch(execution_plan.compute_plan)
        .Build();
}

void RendererSystemSSAO::ClampGlobalParams(SSAOGlobalParams& global_params)
{
    global_params.radius_world = std::clamp(global_params.radius_world, 0.01f, 4.0f);
    global_params.intensity = std::clamp(global_params.intensity, 0.0f, 8.0f);
    global_params.power = std::clamp(global_params.power, 0.1f, 8.0f);
    global_params.bias = std::clamp(global_params.bias, 0.0001f, 1.0f);
    global_params.thickness = std::clamp(global_params.thickness, 0.001f, 2.0f);
    global_params.sample_distribution_power = std::clamp(global_params.sample_distribution_power, 0.25f, 8.0f);
    global_params.blur_depth_reject = std::clamp(global_params.blur_depth_reject, 0.1f, 128.0f);
    global_params.blur_normal_reject = std::clamp(global_params.blur_normal_reject, 0.1f, 256.0f);
    global_params.sample_count = std::clamp(global_params.sample_count, 1u, 32u);
    global_params.blur_radius = std::clamp(global_params.blur_radius, 0u, 3u);
    global_params.enabled = global_params.enabled != 0u ? 1u : 0u;
    global_params.debug_output_mode = std::clamp(global_params.debug_output_mode, 0u, 1u);
}

bool RendererSystemSSAO::ShouldRunBlurPasses() const
{
    return m_global_params.enabled != 0u &&
           m_global_params.blur_radius > 0u &&
           m_global_params.debug_output_mode == 0u;
}

void RendererSystemSSAO::UploadGlobalParams(RendererInterface::ResourceOperator& resource_operator)
{
    if (!m_need_upload_params)
    {
        return;
    }

    RendererInterface::BufferUploadDesc global_params_upload_desc{};
    global_params_upload_desc.data = &m_global_params;
    global_params_upload_desc.size = sizeof(SSAOGlobalParams);
    resource_operator.UploadBufferData(m_ssao_global_params_handle, global_params_upload_desc);

    m_need_upload_params = false;
}
