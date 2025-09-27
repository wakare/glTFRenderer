#include "DemoAppModelViewer.h"

void DemoAppModelViewer::Run(const std::vector<std::string>& arguments)
{
    InitRenderContext(arguments);
    
    // Create shader resource
    auto vertex_shader_handle = CreateShader(RendererInterface::ShaderType::VERTEX_SHADER, "Resources/Shaders/ModelRenderingShader.hlsl", "MainVS");
    auto fragment_shader_handle = CreateShader(RendererInterface::ShaderType::FRAGMENT_SHADER, "Resources/Shaders/ModelRenderingShader.hlsl", "MainFS");

    RendererInterface::RenderSceneDesc render_scene_desc{"glTFResources/Models/Sponza/glTF/Sponza.gltf"};
    RendererInterface::RenderSceneHandle scene = m_resource_manager->CreateRenderScene(render_scene_desc);

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

    render_pass_draw_desc.execute_command.type = RendererInterface::ExecuteCommandType::DRAW_VERTEX_COMMAND;
    render_pass_draw_desc.execute_command.parameter.draw_vertex_command_parameter.vertex_count = 3;
    render_pass_draw_desc.execute_command.parameter.draw_vertex_command_parameter.start_vertex_location = 0;

    RendererInterface::RenderGraphNodeDesc render_graph_node_desc{};
    render_graph_node_desc.draw_info  = render_pass_draw_desc;
    render_graph_node_desc.render_pass_handle = render_pass_handle;

    auto render_graph_node_handle = m_render_graph->CreateRenderGraphNode(render_graph_node_desc);
    m_render_graph->RegisterRenderGraphNode(render_graph_node_handle);

    // After registration all passes, compile graph and prepare for execution
    m_render_graph->CompileRenderPassAndExecute();
    
    m_window->TickWindow();
}
