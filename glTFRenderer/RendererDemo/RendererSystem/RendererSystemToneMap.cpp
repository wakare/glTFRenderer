#include "RendererSystemToneMap.h"

#include "RenderPassSetupBuilder.h"
#include "RendererSystemFrostedGlass.h"
#include "RendererSystemLighting.h"
#include "RendererSystemSceneRenderer.h"
#include <algorithm>
#include <imgui/imgui.h>
#include <utility>

RendererSystemToneMap::RendererSystemToneMap(std::shared_ptr<RendererSystemFrostedGlass> frosted,
                                             std::shared_ptr<RendererSystemLighting> lighting,
                                             std::shared_ptr<RendererSystemSceneRenderer> scene)
    : m_frosted(std::move(frosted))
    , m_lighting(std::move(lighting))
    , m_scene(std::move(scene))
{
}

bool RendererSystemToneMap::Init(RendererInterface::ResourceOperator& resource_operator,
                                 RendererInterface::RenderGraph& graph)
{
    GLTF_CHECK(m_scene);
    GLTF_CHECK(m_scene->HasInit());
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

    const auto lighting_outputs = m_lighting->GetOutputs();
    RendererInterface::RenderTargetHandle input_color = m_frosted
        ? m_frosted->GetOutput()
        : lighting_outputs.output;
    GLTF_CHECK(input_color != NULL_HANDLE);
    const unsigned width = (std::max)(1u, resource_operator.GetCurrentRenderWidth());
    const unsigned height = (std::max)(1u, resource_operator.GetCurrentRenderHeight());
    m_tone_map_pass_node = graph.CreateRenderGraphNode(
        resource_operator,
        BuildToneMapPassSetupInfo(input_color, width, height));
    UploadGlobalParams(resource_operator);
    graph.RegisterRenderTargetToColorOutput(m_tone_map_output);
    return true;
}

RendererInterface::RenderGraph::RenderPassSetupInfo RendererSystemToneMap::BuildToneMapPassSetupInfo(
    RendererInterface::RenderTargetHandle input_color,
    unsigned width,
    unsigned height) const
{
    const auto scene_outputs = m_scene->GetOutputs();
    return RenderFeature::PassBuilder::Compute("Tone Map", "Tone Map Composite")
        .AddShader(RendererInterface::COMPUTE_SHADER, "main", "Resources/Shaders/ToneMap.hlsl")
        .AddSampledRenderTarget(
            "InputColorTex",
            input_color,
            RendererInterface::RenderTargetTextureBindingDesc::SRV)
        .AddSampledRenderTarget(
            "InputVelocityTex",
            scene_outputs.velocity,
            RendererInterface::RenderTargetTextureBindingDesc::SRV)
        .AddSampledRenderTarget(
            "Output",
            m_tone_map_output,
            RendererInterface::RenderTargetTextureBindingDesc::UAV)
        .AddBuffer(
            "ToneMapGlobalBuffer",
            RenderFeature::MakeConstantBufferBinding(m_tone_map_global_params_handle))
        .SetDispatch2D(width, height)
        .Build();
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

    int debug_view_mode = static_cast<int>(m_global_params.debug_view_mode);
    const char* debug_view_modes[] = {"Final", "Velocity"};
    if (ImGui::Combo("Debug View", &debug_view_mode, debug_view_modes, IM_ARRAYSIZE(debug_view_modes)))
    {
        m_global_params.debug_view_mode = static_cast<unsigned>(debug_view_mode);
        params_dirty = true;
    }

    if (m_global_params.debug_view_mode == 1)
    {
        if (ImGui::SliderFloat("Velocity Scale", &m_global_params.debug_velocity_scale, 1.0f, 128.0f, "%.1f"))
        {
            params_dirty = true;
        }
    }

    if (params_dirty)
    {
        const auto clamp = [](float value, float min_value, float max_value) -> float
        {
            return (std::max)(min_value, (std::min)(value, max_value));
        };

        m_global_params.exposure = clamp(m_global_params.exposure, 0.01f, 8.0f);
        m_global_params.gamma = clamp(m_global_params.gamma, 1.0f, 3.0f);
        m_global_params.debug_velocity_scale = clamp(m_global_params.debug_velocity_scale, 0.1f, 256.0f);
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
