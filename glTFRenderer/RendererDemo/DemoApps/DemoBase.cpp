#include "DemoBase.h"

#include "RendererInterface.h"

bool DemoBase::InitRenderContext(const std::vector<std::string>& arguments)
{
    bool bUseDX = true;
    unsigned int width{1280}, height{720};
    
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

    RendererInterface::RenderWindowDesc window_desc{};
    window_desc.width = width;
    window_desc.height = height;
    
    m_window = std::make_shared<RendererInterface::RenderWindow>(window_desc);

    RendererInterface::RenderDeviceDesc device{};
    device.window = m_window->GetHandle();
    device.type = bUseDX ? RendererInterface::DX12 : RendererInterface::VULKAN;
    device.back_buffer_count = 3;

    m_resource_manager = std::make_shared<RendererInterface::ResourceOperator>(device);

    m_render_graph = std::make_shared<RendererInterface::RenderGraph>(*m_resource_manager, *m_window);
}

RendererInterface::ShaderHandle DemoBase::CreateShader(RendererInterface::ShaderType type, const std::string& source,
    const std::string& entry_function)
{
    RendererInterface::ShaderDesc fragment_shader_desc{};
    fragment_shader_desc.shader_type = type; 
    fragment_shader_desc.entry_point = entry_function;
    fragment_shader_desc.shader_file_name = source;
    return m_resource_manager->CreateShader(fragment_shader_desc);
}

RendererInterface::RenderTargetHandle DemoBase::CreateRenderTarget(const std::string& name, unsigned width,
    unsigned height, RendererInterface::PixelFormat format, RendererInterface::RenderTargetClearValue clear_value,
    RendererInterface::ResourceUsage usage)
{
    RendererInterface::RenderTargetDesc render_target_desc{};
    render_target_desc.name = name;
    render_target_desc.width = width;
    render_target_desc.height = height;
    render_target_desc.format = format;
    render_target_desc.clear = clear_value;
    render_target_desc.usage = usage; 
    return m_resource_manager->CreateRenderTarget(render_target_desc);
}
