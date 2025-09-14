#include "DemoTriangleApp.h"

#include "Renderer.h"
#include "RendererInterface.h"

void DemoTriangleApp::Run()
{
    unsigned int width{1280}, height{720};
    
    RendererInterface::RenderWindowDesc window_desc{};
    window_desc.width = width;
    window_desc.height = height;
    RendererInterface::RenderWindow window(window_desc);
    
    RendererInterface::RenderDeviceDesc device{};
    device.window = window.GetHandle();
    device.type = RendererInterface::DX12;
    device.back_buffer_count = 3;
    
    RendererInterface::ResourceAllocator allocator(device);

    // Create shader resource
    RendererInterface::ShaderDesc vertex_shader_desc{};
    vertex_shader_desc.shader_type = RendererInterface::ShaderType::VERTEX_SHADER; 
    vertex_shader_desc.entry_point = "MainVS";
    vertex_shader_desc.shader_file_name = "Resources/Shaders/DemoShader.hlsl";
    auto vertex_shader_handle = allocator.CreateShader(vertex_shader_desc);

    RendererInterface::ShaderDesc fragment_shader_desc{};
    fragment_shader_desc.shader_type = RendererInterface::ShaderType::FRAGMENT_SHADER; 
    fragment_shader_desc.entry_point = "MainFS";
    fragment_shader_desc.shader_file_name = "Resources/Shaders/DemoShader.hlsl";
    auto fragment_shader_handle = allocator.CreateShader(fragment_shader_desc);

    // Create render target resource
    RendererInterface::RenderTargetDesc render_target_desc{};
    render_target_desc.name = "DemoTriangleColorRT";
    render_target_desc.width = width;
    render_target_desc.height = height;
    render_target_desc.format = RendererInterface::RGBA8_UNORM;
    render_target_desc.clear = RendererInterface::default_clear_color;
    auto render_target_handle = allocator.CreateRenderTarget(render_target_desc);
    
    RendererInterface::RenderPassDesc render_pass_desc{};
    render_pass_desc.shaders.emplace(RendererInterface::ShaderType::VERTEX_SHADER, vertex_shader_handle);
    render_pass_desc.shaders.emplace(RendererInterface::ShaderType::FRAGMENT_SHADER, fragment_shader_handle);
    render_pass_desc.type = RendererInterface::GRAPHICS;

    auto render_pass_handle = allocator.CreateRenderPass(render_pass_desc);
    
    RendererInterface::RenderPassDrawDesc render_pass_draw_desc{};
    render_pass_draw_desc.render_target_resources.emplace(render_target_handle,
        RendererInterface::RenderTargetBindingDesc
        {
            .format = render_target_desc.format,
            .usage = RendererInterface::RenderPassResourceUsage::COLOR,
        });

    render_pass_draw_desc.execute_command.type = RendererInterface::ExecuteCommandType::DRAW_VERTEX_COMMAND;
    render_pass_draw_desc.execute_command.parameter.draw_vertex_command_parameter.vertex_count = 3;

    RendererInterface::RenderGraphNodeDesc render_graph_node_desc{};
    render_graph_node_desc.draw_info  = render_pass_draw_desc;
    render_graph_node_desc.render_pass_handle = render_pass_handle;
    
    RendererInterface::RenderGraph graph(allocator, window);
    auto render_graph_node_handle = graph.CreateRenderGraphNode(render_graph_node_desc);
    graph.RegisterRenderGraphNode(render_graph_node_handle);

    // After registration all passes, compile graph and prepare for execution
    graph.CompileRenderPassAndExecute();
    
    window.TickWindow();
}
