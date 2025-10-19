#include "DemoBase.h"

#include "RendererInterface.h"
#include "RendererModule/RendererModuleBase.h"

void DemoBase::TickFrame(unsigned long long time_interval)
{
    TickFrameInternal(time_interval);
    
    for (const auto& module : m_modules)
    {
        module->Tick(*m_resource_manager, time_interval);
    }
}

RendererInterface::RenderGraphNodeHandle DemoBase::SetupPassNode(const RenderPassSetupInfo& setup_info)
{
    RendererInterface::RenderPassDesc render_pass_desc{};
    render_pass_desc.type = setup_info.render_pass_type;
    
    for (const auto& shader_info : setup_info.shader_setup_infos)
    {
        auto shader_handle = CreateShader(shader_info.shader_type, shader_info.shader_file, shader_info.entry_function);
        render_pass_desc.shaders.emplace(shader_info.shader_type, shader_handle);
    }
    
    RendererInterface::RenderPassDrawDesc render_pass_draw_desc{};
    for (const auto& module : setup_info.modules)
    {
        module->BindDrawCommands(render_pass_draw_desc);
    }

    switch (setup_info.render_pass_type)
    {
    case RendererInterface::RenderPassType::GRAPHICS:
        for (const auto& render_target : setup_info.render_targets)
        {
            render_pass_desc.render_target_bindings.push_back(render_target.second);
            render_pass_draw_desc.render_target_resources.emplace(render_target.first, render_target.second);
        }
        break;
    case RendererInterface::RenderPassType::COMPUTE:
        for (const auto& render_target : setup_info.sampled_render_targets)
        {
            render_pass_draw_desc.render_target_texture_resources[render_target.second.name] = render_target.second;
        }
        break;
    case RendererInterface::RenderPassType::RAY_TRACING:
        break;
    }
    
    if (setup_info.execute_command.has_value())
    {
        render_pass_draw_desc.execute_commands.push_back(setup_info.execute_command.value());    
    }
    
    auto render_pass_handle = m_resource_manager->CreateRenderPass(render_pass_desc);
    
    RendererInterface::RenderGraphNodeDesc render_graph_node_desc{};
    render_graph_node_desc.draw_info = render_pass_draw_desc;
    render_graph_node_desc.render_pass_handle = render_pass_handle;

    auto render_graph_node_handle = m_render_graph->CreateRenderGraphNode(render_graph_node_desc);
    m_render_graph->RegisterRenderGraphNode(render_graph_node_handle);

    return render_graph_node_handle;
}

void DemoBase::TickFrameInternal(unsigned long long time_interval)
{
}

bool DemoBase::Init(const std::vector<std::string>& arguments)
{
    RETURN_IF_FALSE(InitRenderContext(arguments))
    
    RETURN_IF_FALSE(InitInternal(arguments))

    for (const auto& module : m_modules)
    {
        module->FinalizeModule(*m_resource_manager);
    }
    
    return true;
}

void DemoBase::Run()
{
    m_window->EnterWindowEventLoop();
}

bool DemoBase::InitRenderContext(const std::vector<std::string>& arguments)
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

    RendererInterface::RenderWindowDesc window_desc{};
    window_desc.width = m_width;
    window_desc.height = m_height;
    
    m_window = std::make_shared<RendererInterface::RenderWindow>(window_desc);

    RendererInterface::RenderDeviceDesc device{};
    device.window = m_window->GetHandle();
    device.type = bUseDX ? RendererInterface::DX12 : RendererInterface::VULKAN;
    device.back_buffer_count = 3;

    m_resource_manager = std::make_shared<RendererInterface::ResourceOperator>(device);

    m_render_graph = std::make_shared<RendererInterface::RenderGraph>(*m_resource_manager, *m_window);
    m_render_graph->RegisterTickCallback([this](unsigned long long time){ TickFrame(time); });

    return true;
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
    RendererInterface::RenderTargetHandle handle = m_resource_manager->CreateRenderTarget(render_target_desc);

    GLTF_CHECK(!m_render_target_desc_infos.contains(handle));
    m_render_target_desc_infos[handle] = render_target_desc;
    
    return handle;
}