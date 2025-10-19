
#include "DemoTriangleApp.h"

#include "Renderer.h"
#include "RendererInterface.h"

bool DemoTriangleApp::InitInternal(const std::vector<std::string>& arguments)
{
    // Create shader resource
    auto vertex_shader_handle = CreateShader(RendererInterface::ShaderType::VERTEX_SHADER, "Resources/Shaders/DemoShader.hlsl", "MainVS");
    auto fragment_shader_handle  = CreateShader(RendererInterface::ShaderType::FRAGMENT_SHADER, "Resources/Shaders/DemoShader.hlsl", "MainFS");

    // Create render target resource
    auto render_target_handle = CreateRenderTarget("DemoTriangleColorRT", m_window->GetWidth(), m_window->GetHeight(), RendererInterface::RGBA8_UNORM, RendererInterface::default_clear_color,
    static_cast<RendererInterface::ResourceUsage>(RendererInterface::ResourceUsage::RENDER_TARGET | RendererInterface::ResourceUsage::COPY_SRC));
    
    RendererInterface::RenderPassDesc render_pass_desc{};
    render_pass_desc.shaders.emplace(RendererInterface::ShaderType::VERTEX_SHADER, vertex_shader_handle);
    render_pass_desc.shaders.emplace(RendererInterface::ShaderType::FRAGMENT_SHADER, fragment_shader_handle);

    RendererInterface::RenderTargetBindingDesc render_target_binding_desc{};
    render_target_binding_desc.format = RendererInterface::RGBA8_UNORM;
    render_target_binding_desc.usage = RendererInterface::RenderPassResourceUsage::COLOR; 
    render_pass_desc.render_target_bindings.push_back(render_target_binding_desc);
    render_pass_desc.type = RendererInterface::RenderPassType::GRAPHICS;

    auto render_pass_handle = m_resource_manager->CreateRenderPass(render_pass_desc);

    // Buffer resources
    RendererInterface::BufferDesc buffer_desc{};
    buffer_desc.name = "ColorBuffer";
    buffer_desc.type = RendererInterface::BufferType::UPLOAD;
    buffer_desc.usage = RendererInterface::BufferUsage::USAGE_CBV;
    
    buffer_desc.size = sizeof(float) * 4; // color - float4
    m_color = {1.0f, 0.0f, 0.0f, 1.0f};
    buffer_desc.data = &m_color;
    
    m_color_buffer_handle = m_resource_manager->CreateBuffer(buffer_desc);
    
    RendererInterface::RenderPassDrawDesc render_pass_draw_desc{};
    render_pass_draw_desc.render_target_resources.emplace(render_target_handle,
        RendererInterface::RenderTargetBindingDesc
        {
            .format = RendererInterface::RGBA8_UNORM,
            .usage = RendererInterface::RenderPassResourceUsage::COLOR,
            .need_clear = true,
        });

    RendererInterface::RenderExecuteCommand execute_command{};
    execute_command.type = RendererInterface::ExecuteCommandType::DRAW_VERTEX_COMMAND;
    execute_command.parameter.draw_vertex_command_parameter.vertex_count = 3;
    execute_command.parameter.draw_vertex_command_parameter.start_vertex_location = 0;
    render_pass_draw_desc.execute_commands.push_back(execute_command);
    
    RendererInterface::BufferBindingDesc buffer_binding_desc{};
    buffer_binding_desc.buffer_handle = m_color_buffer_handle;
    buffer_binding_desc.binding_type = RendererInterface::BufferBindingDesc::BufferBindingType::CBV;
    buffer_binding_desc.is_structured_buffer = false;
    render_pass_draw_desc.buffer_resources["DemoBuffer"] = buffer_binding_desc;
    
    RendererInterface::RenderGraphNodeDesc render_graph_node_desc{};
    render_graph_node_desc.draw_info  = render_pass_draw_desc;
    render_graph_node_desc.render_pass_handle = render_pass_handle;
    
    auto render_graph_node_handle = m_render_graph->CreateRenderGraphNode(render_graph_node_desc);
    m_render_graph->RegisterRenderGraphNode(render_graph_node_handle);

    // After registration all passes, compile graph and prepare for execution
    m_render_graph->CompileRenderPassAndExecute();
    
    return true;
}

void DemoTriangleApp::TickFrameInternal(unsigned long long time_interval)
{
    m_color[0] += 0.05f;
    m_color[1] += 0.03f;
    m_color[2] += 0.01f;
    m_color[0] = m_color[0] > 1.0f ? m_color[0] - 1.0f : m_color[0];
    m_color[1] = m_color[1] > 1.0f ? m_color[1] - 1.0f : m_color[1];
    m_color[2] = m_color[2] > 1.0f ? m_color[2] - 1.0f : m_color[2];
        
    RendererInterface::BufferUploadDesc upload_desc{};
    upload_desc.data = &m_color;
    upload_desc.size = sizeof(float) * 4;
    m_resource_manager->UploadBufferData(m_color_buffer_handle, upload_desc);
}
