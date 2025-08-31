#include "DemoTriangleApp.h"

#include "Renderer.h"
#include "RendererInterface.h"

void DemoTriangleApp::Run()
{
    RendererInterface::RenderWindowDesc window_desc{};
    window_desc.width = 1280;
    window_desc.height = 720;
    RendererInterface::RenderWindow window(window_desc);
    
    RendererInterface::RenderDeviceDesc device{};
    device.window = window.GetHandle();
    device.type = RendererInterface::DX12;
    
    RendererInterface::ResourceAllocator allocator(device);

    // Create shader resource
    RendererInterface::ShaderDesc vertex_shader_desc{};
    vertex_shader_desc.shader_type = RendererInterface::ShaderType::VERTEX_SHADER; 
    vertex_shader_desc.entry_point = "MainVS";
    vertex_shader_desc.shader_file_name = "Resources/Shaders/DemoShader.hlsl";
    auto vertex_shader_handle = allocator.CreateShader(vertex_shader_desc);

    RendererInterface::ShaderDesc fragment_shader_desc{};
    vertex_shader_desc.shader_type = RendererInterface::ShaderType::FRAGMENT_SHADER; 
    vertex_shader_desc.entry_point = "MainFS";
    vertex_shader_desc.shader_file_name = "Resources/Shaders/DemoShader.hlsl";
    auto fragment_shader_handle = allocator.CreateShader(vertex_shader_desc);

    // Create render target resource
    RendererInterface::RenderTargetDesc render_target_desc{};
    render_target_desc.width = 1280;
    render_target_desc.height = 720;
    render_target_desc.format = RendererInterface::RGBA8;
    auto render_target_handle = allocator.CreateRenderTarget(render_target_desc);
    
    RendererInterface::RenderPassDesc render_pass_desc{};
    render_pass_desc.shaders.emplace(RendererInterface::ShaderType::VERTEX_SHADER, vertex_shader_handle);
    render_pass_desc.shaders.emplace(RendererInterface::ShaderType::FRAGMENT_SHADER, fragment_shader_handle);
    render_pass_desc.type = RendererInterface::GRAPHICS;
    render_pass_desc.render_target_access_modes.emplace(render_target_handle, RendererInterface::RenderPassAccessMode::WRITE_ONLY);
    
    window.TickWindow();
}
