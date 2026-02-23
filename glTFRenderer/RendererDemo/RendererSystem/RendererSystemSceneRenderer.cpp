#include "RendererSystemSceneRenderer.h"

RendererSystemSceneRenderer::RendererSystemSceneRenderer(RendererInterface::ResourceOperator& resource_operator, const RendererCameraDesc& camera_desc, const std::string& scene_file)
{
    m_camera_module = std::make_shared<RendererModuleCamera>(resource_operator, camera_desc);
    m_scene_mesh_module = std::make_shared<RendererModuleSceneMesh>(resource_operator, scene_file);

    m_modules.push_back(m_scene_mesh_module);
    m_modules.push_back(m_camera_module);
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

bool RendererSystemSceneRenderer::Init(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph)
{
    m_base_pass_color = resource_operator.CreateWindowRelativeRenderTarget("BasePass_Color", RendererInterface::RGBA8_UNORM, RendererInterface::default_clear_color,
        static_cast<RendererInterface::ResourceUsage>(RendererInterface::ResourceUsage::RENDER_TARGET | RendererInterface::ResourceUsage::COPY_SRC | RendererInterface::ResourceUsage::SHADER_RESOURCE));

    m_base_pass_normal = resource_operator.CreateWindowRelativeRenderTarget("BasePass_Normal", RendererInterface::RGBA8_UNORM, RendererInterface::default_clear_color,
        static_cast<RendererInterface::ResourceUsage>(RendererInterface::ResourceUsage::RENDER_TARGET | RendererInterface::ResourceUsage::COPY_SRC | RendererInterface::ResourceUsage::SHADER_RESOURCE));

    m_base_pass_depth = resource_operator.CreateWindowRelativeRenderTarget("Depth", RendererInterface::D32, RendererInterface::default_clear_depth,
        static_cast<RendererInterface::ResourceUsage>(RendererInterface::ResourceUsage::DEPTH_STENCIL | RendererInterface::ResourceUsage::SHADER_RESOURCE));
    
    // Setup base pass rendering config
    {
        RendererInterface::RenderGraph::RenderPassSetupInfo setup_info{};
        setup_info.render_pass_type = RendererInterface::RenderPassType::GRAPHICS;
        setup_info.debug_group = "Scene Renderer";
        setup_info.debug_name = "Base Pass";
        setup_info.modules = {m_scene_mesh_module, m_camera_module};
        setup_info.shader_setup_infos = {
            {
                .shader_type = RendererInterface::ShaderType::VERTEX_SHADER,
                .entry_function = "MainVS",
                .shader_file = "Resources/Shaders/ModelRenderingShader.hlsl"
            },
            {
                .shader_type = RendererInterface::ShaderType::FRAGMENT_SHADER,
                .entry_function = "MainFS",
                .shader_file = "Resources/Shaders/ModelRenderingShader.hlsl"
            }
        };

        RendererInterface::RenderTargetBindingDesc color_rt_binding_desc{};
        color_rt_binding_desc.format = RendererInterface::RGBA8_UNORM;
        color_rt_binding_desc.usage = RendererInterface::RenderPassResourceUsage::COLOR;
        color_rt_binding_desc.need_clear = true;

        RendererInterface::RenderTargetBindingDesc normal_rt_binding_desc{};
        normal_rt_binding_desc.format = RendererInterface::RGBA8_UNORM;
        normal_rt_binding_desc.usage = RendererInterface::RenderPassResourceUsage::COLOR;
        normal_rt_binding_desc.need_clear = true;

        RendererInterface::RenderTargetBindingDesc depth_binding_desc{};
        depth_binding_desc.format = RendererInterface::D32;
        depth_binding_desc.usage = RendererInterface::RenderPassResourceUsage::DEPTH_STENCIL;
        depth_binding_desc.need_clear = true;
        
        setup_info.render_targets = {
            {m_base_pass_color, color_rt_binding_desc},
            {m_base_pass_normal, normal_rt_binding_desc},
            {m_base_pass_depth, depth_binding_desc}
        };

        m_base_pass_node = graph.CreateRenderGraphNode(resource_operator, setup_info);
    }
    
    return true;
}

bool RendererSystemSceneRenderer::HasInit() const
{
    return m_base_pass_node != NULL_HANDLE;
}

bool RendererSystemSceneRenderer::Tick(RendererInterface::ResourceOperator& resource_operator,
                               RendererInterface::RenderGraph& graph, unsigned long long interval)
{
    graph.RegisterRenderGraphNode(m_base_pass_node);
    
    m_camera_module->Tick(resource_operator, interval);
    m_scene_mesh_module->Tick(resource_operator, interval);
    
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
