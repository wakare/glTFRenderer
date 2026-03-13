#include "RendererSystemSceneRenderer.h"

#include "RenderPassSetupBuilder.h"

void RendererSystemSceneRenderer::BasePassRuntimeState::Reset()
{
    color = NULL_HANDLE;
    normal = NULL_HANDLE;
    velocity = NULL_HANDLE;
    depth = NULL_HANDLE;
    node = NULL_HANDLE;
}

bool RendererSystemSceneRenderer::BasePassRuntimeState::HasInit() const
{
    return node != NULL_HANDLE;
}

RendererSystemSceneRenderer::RendererSystemSceneRenderer(RendererInterface::ResourceOperator& resource_operator, const RendererCameraDesc& camera_desc, const std::string& scene_file)
    : m_camera_desc(camera_desc)
    , m_scene_file(scene_file)
    , m_base_pass_render_state(CreateDefaultBasePassRenderState())
{
    ResetRuntimeResources(resource_operator);
}

void RendererSystemSceneRenderer::UpdateInputDeviceInfo(RendererInputDevice& input_device, unsigned long long interval)
{
    m_camera_module->TickCameraOperation(input_device, interval);
}

unsigned RendererSystemSceneRenderer::GetWidth() const
{
    return m_camera_module->GetWidth();
}

unsigned RendererSystemSceneRenderer::GetHeight() const
{
    return m_camera_module->GetHeight();
}

RendererSystemSceneRenderer::BasePassOutputs RendererSystemSceneRenderer::GetOutputs() const
{
    return BasePassOutputs{
        .color = m_base_pass_state.color,
        .normal = m_base_pass_state.normal,
        .velocity = m_base_pass_state.velocity,
        .depth = m_base_pass_state.depth,
        .node = m_base_pass_state.node
    };
}

const RendererInterface::RenderStateDesc& RendererSystemSceneRenderer::GetBasePassRenderState() const
{
    return m_base_pass_render_state;
}

bool RendererSystemSceneRenderer::SetBasePassRenderState(const RendererInterface::RenderStateDesc& render_state)
{
    const bool current_matches = RendererInterface::IsEquivalentRenderStateDesc(m_base_pass_render_state, render_state);
    const bool pending_matches = m_pending_base_pass_render_state.has_value() &&
        RendererInterface::IsEquivalentRenderStateDesc(*m_pending_base_pass_render_state, render_state);
    if (current_matches && (!m_pending_base_pass_render_state.has_value() || pending_matches))
    {
        return false;
    }

    m_base_pass_render_state = render_state;
    m_pending_base_pass_render_state = render_state;
    return true;
}

RendererInterface::RenderStateDesc RendererSystemSceneRenderer::CreateDefaultBasePassRenderState()
{
    return {};
}

void RendererSystemSceneRenderer::CreateBasePassRenderTargets(RendererInterface::ResourceOperator& resource_operator)
{
    m_base_pass_state.color = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget("BasePass_Color", RendererInterface::RGBA8_UNORM, RendererInterface::default_clear_color,
        static_cast<RendererInterface::ResourceUsage>(RendererInterface::ResourceUsage::RENDER_TARGET | RendererInterface::ResourceUsage::COPY_SRC | RendererInterface::ResourceUsage::SHADER_RESOURCE));

    m_base_pass_state.normal = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget("BasePass_Normal", RendererInterface::RGBA8_UNORM, RendererInterface::default_clear_color,
        static_cast<RendererInterface::ResourceUsage>(RendererInterface::ResourceUsage::RENDER_TARGET | RendererInterface::ResourceUsage::COPY_SRC | RendererInterface::ResourceUsage::SHADER_RESOURCE));

    m_base_pass_state.velocity = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget("BasePass_Velocity", RendererInterface::RGBA16_FLOAT, RendererInterface::default_clear_color,
        static_cast<RendererInterface::ResourceUsage>(RendererInterface::ResourceUsage::RENDER_TARGET | RendererInterface::ResourceUsage::COPY_SRC | RendererInterface::ResourceUsage::SHADER_RESOURCE));

    m_base_pass_state.depth = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget("Depth", RendererInterface::D32, RendererInterface::default_clear_depth,
        static_cast<RendererInterface::ResourceUsage>(RendererInterface::ResourceUsage::DEPTH_STENCIL | RendererInterface::ResourceUsage::SHADER_RESOURCE));
}

RendererInterface::RenderGraph::RenderPassSetupInfo RendererSystemSceneRenderer::BuildBasePassSetupInfo() const
{
    return RenderFeature::PassBuilder::Graphics("Scene Renderer", "Base Pass")
        .SetRenderState(m_base_pass_render_state)
        .AddModules({m_scene_mesh_module, m_camera_module})
        .AddShader(RendererInterface::ShaderType::VERTEX_SHADER, "MainVS", "Resources/Shaders/ModelRenderingShader.hlsl")
        .AddShader(RendererInterface::ShaderType::FRAGMENT_SHADER, "MainFS", "Resources/Shaders/ModelRenderingShader.hlsl")
        .AddRenderTarget(
            m_base_pass_state.color,
            RenderFeature::MakeColorRenderTargetBinding(RendererInterface::RGBA8_UNORM, true))
        .AddRenderTarget(
            m_base_pass_state.normal,
            RenderFeature::MakeColorRenderTargetBinding(RendererInterface::RGBA8_UNORM, true))
        .AddRenderTarget(
            m_base_pass_state.velocity,
            RenderFeature::MakeColorRenderTargetBinding(RendererInterface::RGBA16_FLOAT, true))
        .AddRenderTarget(
            m_base_pass_state.depth,
            RenderFeature::MakeDepthRenderTargetBinding(RendererInterface::D32, true))
        .Build();
}

bool RendererSystemSceneRenderer::Init(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph)
{
    CreateBasePassRenderTargets(resource_operator);
    m_base_pass_state.node = graph.CreateRenderGraphNode(resource_operator, BuildBasePassSetupInfo());
    return true;
}

bool RendererSystemSceneRenderer::HasInit() const
{
    return m_base_pass_state.HasInit();
}

void RendererSystemSceneRenderer::ResetRuntimeResources(RendererInterface::ResourceOperator& resource_operator)
{
    bool has_camera_pose = false;
    glm::fvec3 camera_position{};
    glm::fvec3 camera_euler_angles{};
    unsigned viewport_width = 0;
    unsigned viewport_height = 0;
    if (m_camera_module)
    {
        has_camera_pose = m_camera_module->GetCameraPose(camera_position, camera_euler_angles);
        viewport_width = m_camera_module->GetWidth();
        viewport_height = m_camera_module->GetHeight();
    }

    if (viewport_width == 0 || viewport_height == 0)
    {
        viewport_width = resource_operator.GetCurrentRenderWidth();
        viewport_height = resource_operator.GetCurrentRenderHeight();
    }
    if (viewport_width == 0 || viewport_height == 0)
    {
        viewport_width = static_cast<unsigned>(m_camera_desc.projection_width);
        viewport_height = static_cast<unsigned>(m_camera_desc.projection_height);
    }

    RendererCameraDesc next_camera_desc = m_camera_desc;
    next_camera_desc.projection_width = static_cast<float>(viewport_width);
    next_camera_desc.projection_height = static_cast<float>(viewport_height);

    m_camera_module = std::make_shared<RendererModuleCamera>(resource_operator, next_camera_desc);
    if (has_camera_pose)
    {
        m_camera_module->SetCameraPose(camera_position, camera_euler_angles, true);
    }
    if (viewport_width > 0 && viewport_height > 0)
    {
        m_camera_module->SetViewportSize(viewport_width, viewport_height);
    }

    m_scene_mesh_module = std::make_shared<RendererModuleSceneMesh>(resource_operator, m_scene_file);
    m_modules.clear();
    m_modules.push_back(m_scene_mesh_module);
    m_modules.push_back(m_camera_module);

    m_base_pass_state.Reset();
    m_camera_desc = next_camera_desc;
}

bool RendererSystemSceneRenderer::Tick(RendererInterface::ResourceOperator& resource_operator,
                               RendererInterface::RenderGraph& graph, unsigned long long interval)
{
    QueuePendingBasePassRenderStateUpdate(graph);
    graph.RegisterRenderGraphNode(m_base_pass_state.node);
    
    m_camera_module->Tick(resource_operator, interval);
    m_scene_mesh_module->Tick(resource_operator, interval);
    
    return true;
}

bool RendererSystemSceneRenderer::QueuePendingBasePassRenderStateUpdate(RendererInterface::RenderGraph& graph)
{
    if (!m_pending_base_pass_render_state.has_value() || m_base_pass_state.node == NULL_HANDLE)
    {
        return true;
    }

    if (!graph.QueueNodeRenderStateUpdate(m_base_pass_state.node, *m_pending_base_pass_render_state))
    {
        return false;
    }

    m_pending_base_pass_render_state.reset();
    return true;
}

void RendererSystemSceneRenderer::OnResize(RendererInterface::ResourceOperator& resource_operator, unsigned width, unsigned height)
{
    (void)resource_operator;
    if (m_camera_module)
    {
        m_camera_module->SetViewportSize(width, height);
    }
}

std::shared_ptr<RendererModuleCamera> RendererSystemSceneRenderer::GetCameraModule() const
{
    return m_camera_module;
}

std::shared_ptr<RendererModuleSceneMesh> RendererSystemSceneRenderer::GetSceneMeshModule() const
{
    return m_scene_mesh_module;
}
