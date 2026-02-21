#include "DemoBase.h"

#include "RendererInterface.h"
#include "RendererSystem/RendererSystemBase.h"
#include <imgui/imgui.h>
#include <map>

void DemoBase::TickFrame(unsigned long long time_interval)
{
    TickFrameInternal(time_interval);
    
    for (const auto& module : m_modules)
    {
        module->Tick(*m_resource_manager, time_interval);
    }

    for (auto& system : m_systems)
    {
        system->Tick(*m_resource_manager, *m_render_graph, time_interval);
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
        break;
    case RendererInterface::RenderPassType::RAY_TRACING:
        break;
    }
    
    for (const auto& render_target : setup_info.sampled_render_targets)
    {
        render_pass_draw_desc.render_target_texture_resources[render_target.second.name] = render_target.second;
    }
    
    if (setup_info.execute_command.has_value())
    {
        render_pass_draw_desc.execute_commands.push_back(setup_info.execute_command.value());    
    }
    
    auto render_pass_handle = m_resource_manager->CreateRenderPass(render_pass_desc);
    
    RendererInterface::RenderGraphNodeDesc render_graph_node_desc{};
    render_graph_node_desc.draw_info = render_pass_draw_desc;
    render_graph_node_desc.render_pass_handle = render_pass_handle;
    render_graph_node_desc.debug_group = setup_info.debug_group.empty() ? "Demo" : setup_info.debug_group;
    render_graph_node_desc.debug_name = setup_info.debug_name;

    auto render_graph_node_handle = m_render_graph->CreateRenderGraphNode(render_graph_node_desc);
    m_render_graph->RegisterRenderGraphNode(render_graph_node_handle);

    return render_graph_node_handle;
}

void DemoBase::TickFrameInternal(unsigned long long time_interval)
{
}

void DemoBase::DrawDebugUI()
{
    ImGui::Begin(GetDemoPanelName());

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("Frame %.3f ms (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::Separator();

    if (m_render_graph && ImGui::CollapsingHeader("Pass Timings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const auto& frame_stats = m_render_graph->GetLastFrameStats();
        if (frame_stats.gpu_time_valid)
        {
            ImGui::Text("RenderGraph CPU: %.3f ms | GPU: %.3f ms | Passes: %u | Frame: %llu",
                frame_stats.cpu_total_ms,
                frame_stats.gpu_total_ms,
                static_cast<unsigned>(frame_stats.pass_stats.size()),
                frame_stats.frame_index);
        }
        else
        {
            ImGui::Text("RenderGraph CPU: %.3f ms | GPU: N/A | Passes: %u | Frame: %llu",
                frame_stats.cpu_total_ms,
                static_cast<unsigned>(frame_stats.pass_stats.size()),
                frame_stats.frame_index);
        }

        if (!frame_stats.pass_stats.empty())
        {
            std::map<std::string, float> group_totals;
            std::map<std::string, float> group_gpu_totals;
            std::map<std::string, bool> group_gpu_valid;
            for (const auto& pass_stat : frame_stats.pass_stats)
            {
                group_totals[pass_stat.group_name] += pass_stat.cpu_time_ms;
                if (pass_stat.gpu_time_valid)
                {
                    group_gpu_totals[pass_stat.group_name] += pass_stat.gpu_time_ms;
                    group_gpu_valid[pass_stat.group_name] = true;
                }
            }

            for (const auto& group_total : group_totals)
            {
                if (group_gpu_valid[group_total.first])
                {
                    ImGui::Text("%s: CPU %.3f ms | GPU %.3f ms",
                        group_total.first.c_str(),
                        group_total.second,
                        group_gpu_totals[group_total.first]);
                }
                else
                {
                    ImGui::Text("%s: CPU %.3f ms | GPU N/A", group_total.first.c_str(), group_total.second);
                }
            }

            const ImGuiTableFlags table_flags =
                ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_SizingStretchProp |
                ImGuiTableFlags_ScrollY;
            if (ImGui::BeginTable("RenderPassTimingTable", 4, table_flags, ImVec2(0.0f, 220.0f)))
            {
                ImGui::TableSetupColumn("System");
                ImGui::TableSetupColumn("Pass");
                ImGui::TableSetupColumn("CPU ms");
                ImGui::TableSetupColumn("GPU ms");
                ImGui::TableHeadersRow();

                for (const auto& pass_stat : frame_stats.pass_stats)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(pass_stat.group_name.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(pass_stat.pass_name.c_str());
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%.3f", pass_stat.cpu_time_ms);
                    ImGui::TableSetColumnIndex(3);
                    if (pass_stat.gpu_time_valid)
                    {
                        ImGui::Text("%.3f", pass_stat.gpu_time_ms);
                    }
                    else
                    {
                        ImGui::TextUnformatted("N/A");
                    }
                }
                ImGui::EndTable();
            }
        }
        else
        {
            ImGui::TextUnformatted("No pass timing data this frame.");
        }

        ImGui::Separator();
    }

    DrawDebugUIInternal();

    for (const auto& system : m_systems)
    {
        if (!system)
        {
            continue;
        }

        if (ImGui::CollapsingHeader(system->GetSystemName(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            system->DrawDebugUI();
        }
    }

    ImGui::End();
}

bool DemoBase::Init(const std::vector<std::string>& arguments)
{
    RETURN_IF_FALSE(InitRenderContext(arguments))
    
    RETURN_IF_FALSE(InitInternal(arguments))

    for (const auto& module : m_modules)
    {
        module->FinalizeModule(*m_resource_manager);
    }

    for (const auto& system : m_systems)
    {
        system->FinalizeModule(*m_resource_manager);
        system->Init(*m_resource_manager, *m_render_graph);
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
    m_render_graph->RegisterDebugUICallback([this]() { DrawDebugUI(); });

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
