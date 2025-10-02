#include "DemoAppModelViewer.h"

#include "RenderWindow/RendererInputDevice.h"
#include "SceneRendererUtil/SceneRendererMeshDrawDispatcher.h"
#include <glm/glm/gtx/norm.hpp>

#include "RendererCommon.h"

void DemoAppModelViewer::TickFrame(unsigned long long delta_time_ms)
{
    auto& input_device = m_window->GetInputDevice();
    if (m_camera_operator)
    {
        m_camera_operator->TickCameraOperation(input_device, *m_resource_manager, delta_time_ms);    
    }

    input_device.TickFrame(delta_time_ms);
}

void DemoAppModelViewer::Run(const std::vector<std::string>& arguments)
{
    InitRenderContext(arguments);

    RendererCameraDesc camera_desc{};
    camera_desc.mode = CameraMode::Free;
    camera_desc.transform = glm::mat4(1.0f);
    camera_desc.fov_angle = 90.0f;
    camera_desc.projection_far = 1000.0f;
    camera_desc.projection_near = 0.001f;
    camera_desc.projection_width = static_cast<float>(m_width);
    camera_desc.projection_height = static_cast<float>(m_height);
    m_camera_operator = std::make_unique<SceneRendererCameraOperator>(*m_resource_manager, camera_desc);
    m_draw_dispatcher = std::make_unique<SceneRendererMeshDrawDispatcher>(*m_resource_manager, "glTFResources/Models/Sponza/glTF/Sponza.gltf");
    
    // Create shader resource
    auto vertex_shader_handle = CreateShader(RendererInterface::ShaderType::VERTEX_SHADER, "Resources/Shaders/ModelRenderingShader.hlsl", "MainVS");
    auto fragment_shader_handle = CreateShader(RendererInterface::ShaderType::FRAGMENT_SHADER, "Resources/Shaders/ModelRenderingShader.hlsl", "MainFS");

    // Create render target resource
    auto render_target_handle = CreateRenderTarget("DemoModelViewerColorRT", m_window->GetWidth(), m_window->GetHeight(), RendererInterface::RGBA8_UNORM, RendererInterface::default_clear_color,
    static_cast<RendererInterface::ResourceUsage>(RendererInterface::ResourceUsage::RENDER_TARGET | RendererInterface::ResourceUsage::COPY_SRC));

    RendererInterface::RenderPassDesc render_pass_desc{};
    render_pass_desc.shaders.emplace(RendererInterface::ShaderType::VERTEX_SHADER, vertex_shader_handle);
    render_pass_desc.shaders.emplace(RendererInterface::ShaderType::FRAGMENT_SHADER, fragment_shader_handle);

    RendererInterface::RenderTargetBindingDesc render_target_binding_desc{};
    render_target_binding_desc.format = RendererInterface::RGBA8_UNORM;
    render_target_binding_desc.usage = RendererInterface::RenderPassResourceUsage::COLOR; 
    render_pass_desc.render_target_bindings.push_back(render_target_binding_desc);
    render_pass_desc.type = RendererInterface::GRAPHICS;

    auto render_pass_handle = m_resource_manager->CreateRenderPass(render_pass_desc);
    
    RendererInterface::RenderPassDrawDesc render_pass_draw_desc{};
    render_pass_draw_desc.render_target_resources.emplace(render_target_handle,
        RendererInterface::RenderTargetBindingDesc
        {
            .format = RendererInterface::RGBA8_UNORM,
            .usage = RendererInterface::RenderPassResourceUsage::COLOR,
        });
    render_pass_draw_desc.render_target_clear_states.emplace(render_target_handle, true);
    
    m_draw_dispatcher->BindDrawCommands(render_pass_draw_desc);
    m_camera_operator->BindDrawCommands(render_pass_draw_desc);

    RendererInterface::RenderGraphNodeDesc render_graph_node_desc{};
    render_graph_node_desc.draw_info = render_pass_draw_desc;
    render_graph_node_desc.render_pass_handle = render_pass_handle;
    render_graph_node_desc.pre_render_callback = [this](unsigned long long interval){TickFrame(interval);};

    auto render_graph_node_handle = m_render_graph->CreateRenderGraphNode(render_graph_node_desc);
    m_render_graph->RegisterRenderGraphNode(render_graph_node_handle);

    // After registration all passes, compile graph and prepare for execution
    m_render_graph->CompileRenderPassAndExecute();
    
    m_window->EnterWindowEventLoop();
}