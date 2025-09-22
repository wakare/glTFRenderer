#include "DemoTriangleApp.h"

#include "Renderer.h"
#include "RendererInterface.h"

void DemoTriangleApp::Run(const std::vector<std::string>& arguments)
{
    bool bUseDX = true;
    for (const auto& argument : arguments)
    {
        if (argument == "-dx"|| argument == "-dx12")
        {
            bUseDX = true;
        }

        if (argument == "-vk" || argument == "-vulkan")
        {
            bUseDX = false;
        }
    }
    
    unsigned int width{1280}, height{720};

    InitRenderContext(width, height, bUseDX);
    
    // Create shader resource
    auto vertex_shader_handle = CreateShader(RendererInterface::ShaderType::VERTEX_SHADER, "Resources/Shaders/DemoShader.hlsl", "MainVS");
    auto fragment_shader_handle  = CreateShader(RendererInterface::ShaderType::FRAGMENT_SHADER, "Resources/Shaders/DemoShader.hlsl", "MainFS");

    // Create render target resource
    auto render_target_handle = CreateRenderTarget("DemoTriangleColorRT", width, height, RendererInterface::RGBA8_UNORM, RendererInterface::default_clear_color,
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

    // Buffer resources
    RendererInterface::BufferDesc buffer_desc{};
    buffer_desc.name = "ColorBuffer";
    buffer_desc.type = RendererInterface::BufferType::UPLOAD;
    buffer_desc.usage = RendererInterface::BufferUsage::USAGE_CBV;
    
    buffer_desc.size = sizeof(float) * 4; // color - float4
    buffer_desc.data = std::make_unique<char[]>(buffer_desc.size);
    float color[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    memcpy(buffer_desc.data.value().get(), color, buffer_desc.size);
    
    RendererInterface::BufferHandle buffer_handle = m_resource_manager->CreateBuffer(buffer_desc);
    
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

    RendererInterface::BufferBindingDesc buffer_binding_desc{};
    buffer_binding_desc.buffer_handle = buffer_handle;
    buffer_binding_desc.binding_type = RendererInterface::BufferBindingDesc::BufferBindingType::CBV;
    buffer_binding_desc.is_structured_buffer = false;
    render_pass_draw_desc.buffer_resources["DemoBuffer"] = buffer_binding_desc;
    
    RendererInterface::RenderGraphNodeDesc render_graph_node_desc{};
    render_graph_node_desc.draw_info  = render_pass_draw_desc;
    render_graph_node_desc.render_pass_handle = render_pass_handle;
    render_graph_node_desc.pre_render_callback = [&]()
    {
        color[0] += 0.05f;
        color[1] += 0.03f;
        color[2] += 0.01f;
        color[0] = color[0] > 1.0f ? color[0] - 1.0f : color[0];
        color[1] = color[1] > 1.0f ? color[1] - 1.0f : color[1];
        color[2] = color[2] > 1.0f ? color[2] - 1.0f : color[2];
        
        memcpy(buffer_desc.data.value().get(), &color, sizeof(float) * 4);
        RendererInterface::BufferUploadDesc upload_desc{};
        upload_desc.data = buffer_desc.data.value().get();
        upload_desc.size = sizeof(float) * 4;
        m_resource_manager->UploadBufferData(buffer_handle, upload_desc);
    };
    
    auto render_graph_node_handle = m_render_graph->CreateRenderGraphNode(render_graph_node_desc);
    m_render_graph->RegisterRenderGraphNode(render_graph_node_handle);

    // After registration all passes, compile graph and prepare for execution
    m_render_graph->CompileRenderPassAndExecute();
    
    m_window->TickWindow();
}
