#include "DemoBase.h"

#include "RendererInterface.h"
#include "RendererSystem/RendererSystemBase.h"
#include <imgui/imgui.h>
#include <map>

namespace
{
    RendererInterface::SwapchainResizePolicy GetDefaultSwapchainResizePolicy(RendererInterface::RenderDeviceType type)
    {
        RendererInterface::SwapchainResizePolicy policy{};
        if (type == RendererInterface::DX12)
        {
            policy.min_stable_frames_before_resize = 2;
            policy.retry_cooldown_base_frames = 6;
            policy.retry_cooldown_max_frames = 120;
            policy.retry_log_period = 30;
            policy.use_exponential_retry_backoff = true;
            return policy;
        }

        policy.min_stable_frames_before_resize = 1;
        policy.retry_cooldown_base_frames = 2;
        policy.retry_cooldown_max_frames = 30;
        policy.retry_log_period = 60;
        policy.use_exponential_retry_backoff = true;
        return policy;
    }

    const char* ToString(RendererInterface::RenderDeviceType type)
    {
        switch (type)
        {
        case RendererInterface::DX12:
            return "DX12";
        case RendererInterface::VULKAN:
            return "Vulkan";
        default:
            return "Unknown";
        }
    }

    const char* ToString(RendererInterface::SwapchainLifecycleState state)
    {
        switch (state)
        {
        case RendererInterface::SwapchainLifecycleState::UNINITIALIZED:
            return "UNINITIALIZED";
        case RendererInterface::SwapchainLifecycleState::READY:
            return "READY";
        case RendererInterface::SwapchainLifecycleState::RESIZE_PENDING:
            return "RESIZE_PENDING";
        case RendererInterface::SwapchainLifecycleState::RESIZE_DEFERRED:
            return "RESIZE_DEFERRED";
        case RendererInterface::SwapchainLifecycleState::RECOVERING:
            return "RECOVERING";
        case RendererInterface::SwapchainLifecycleState::MINIMIZED:
            return "MINIMIZED";
        case RendererInterface::SwapchainLifecycleState::INVALID:
            return "INVALID";
        default:
            return "UNKNOWN";
        }
    }
}

void DemoBase::TickFrame(unsigned long long time_interval)
{
    if (m_resource_manager)
    {
        const unsigned render_width = m_resource_manager->GetCurrentRenderWidth();
        const unsigned render_height = m_resource_manager->GetCurrentRenderHeight();
        if (render_width > 0 && render_height > 0 &&
            (render_width != m_last_render_width || render_height != m_last_render_height))
        {
            m_last_render_width = render_width;
            m_last_render_height = render_height;

            for (auto& system : m_systems)
            {
                if (!system)
                {
                    continue;
                }
                system->OnResize(*m_resource_manager, render_width, render_height);
            }
        }
    }

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

    if (ImGui::CollapsingHeader("Advance"))
    {
        if (m_resource_manager && ImGui::CollapsingHeader("Surface / Swapchain"))
        {
            if (!m_swapchain_resize_policy_ui_initialized)
            {
                m_swapchain_resize_policy_ui = m_resource_manager->GetSwapchainResizePolicy();
                m_swapchain_resize_policy_ui_initialized = true;
            }

            ImGui::Text("API: %s", ToString(m_render_device_type));
            ImGui::Text("Render Extent: %u x %u",
                m_resource_manager->GetCurrentRenderWidth(),
                m_resource_manager->GetCurrentRenderHeight());
            ImGui::Text("Lifecycle: %s", ToString(m_resource_manager->GetSwapchainLifecycleState()));

            bool policy_changed = false;
            int min_stable_frames_before_resize = static_cast<int>(m_swapchain_resize_policy_ui.min_stable_frames_before_resize);
            int retry_cooldown_base_frames = static_cast<int>(m_swapchain_resize_policy_ui.retry_cooldown_base_frames);
            int retry_cooldown_max_frames = static_cast<int>(m_swapchain_resize_policy_ui.retry_cooldown_max_frames);
            int retry_log_period = static_cast<int>(m_swapchain_resize_policy_ui.retry_log_period);
            bool use_exponential_retry_backoff = m_swapchain_resize_policy_ui.use_exponential_retry_backoff;

            if (ImGui::SliderInt("Stable Frames Before Resize", &min_stable_frames_before_resize, 1, 12))
            {
                m_swapchain_resize_policy_ui.min_stable_frames_before_resize = static_cast<unsigned>(min_stable_frames_before_resize);
                policy_changed = true;
            }
            if (ImGui::SliderInt("Retry Cooldown Base (frames)", &retry_cooldown_base_frames, 1, 60))
            {
                m_swapchain_resize_policy_ui.retry_cooldown_base_frames = static_cast<unsigned>(retry_cooldown_base_frames);
                if (m_swapchain_resize_policy_ui.retry_cooldown_max_frames < m_swapchain_resize_policy_ui.retry_cooldown_base_frames)
                {
                    m_swapchain_resize_policy_ui.retry_cooldown_max_frames = m_swapchain_resize_policy_ui.retry_cooldown_base_frames;
                }
                policy_changed = true;
            }
            if (ImGui::SliderInt("Retry Cooldown Max (frames)", &retry_cooldown_max_frames, 1, 240))
            {
                m_swapchain_resize_policy_ui.retry_cooldown_max_frames = static_cast<unsigned>(retry_cooldown_max_frames);
                if (m_swapchain_resize_policy_ui.retry_cooldown_max_frames < m_swapchain_resize_policy_ui.retry_cooldown_base_frames)
                {
                    m_swapchain_resize_policy_ui.retry_cooldown_max_frames = m_swapchain_resize_policy_ui.retry_cooldown_base_frames;
                }
                policy_changed = true;
            }
            if (ImGui::SliderInt("Resize Failure Log Period", &retry_log_period, 1, 240))
            {
                m_swapchain_resize_policy_ui.retry_log_period = static_cast<unsigned>(retry_log_period);
                policy_changed = true;
            }
            if (ImGui::Checkbox("Use Exponential Backoff", &use_exponential_retry_backoff))
            {
                m_swapchain_resize_policy_ui.use_exponential_retry_backoff = use_exponential_retry_backoff;
                policy_changed = true;
            }

            if (policy_changed)
            {
                m_resource_manager->SetSwapchainResizePolicy(m_swapchain_resize_policy_ui, true);
            }

            if (ImGui::Button("Reset To API Defaults"))
            {
                m_swapchain_resize_policy_ui = GetDefaultSwapchainResizePolicy(m_render_device_type);
                m_resource_manager->SetSwapchainResizePolicy(m_swapchain_resize_policy_ui, true);
            }
            ImGui::SameLine();
            if (ImGui::Button("Force Surface Resync"))
            {
                m_resource_manager->InvalidateSwapchainResizeRequest();
            }
        }

        if (m_render_graph && ImGui::CollapsingHeader("RenderGraph / Dependency Diagnostics"))
        {
            const auto& diagnostics = m_render_graph->GetDependencyDiagnostics();
            ImGui::Text("Graph Valid: %s", diagnostics.graph_valid ? "Yes" : "No");
            ImGui::Text("Auto-Merged Inferred Edges: %u", diagnostics.auto_merged_dependency_count);
            ImGui::Text("Invalid Explicit Edges: %u", static_cast<unsigned>(diagnostics.invalid_explicit_dependencies.size()));
            ImGui::Text("Cycle Nodes: %u", static_cast<unsigned>(diagnostics.cycle_nodes.size()));
            ImGui::Text("Execution Signature: %zu", diagnostics.execution_signature);
            ImGui::Text("Cached Nodes: %zu | Cached Order Size: %zu",
                diagnostics.cached_execution_node_count,
                diagnostics.cached_execution_order_size);

            if (!diagnostics.invalid_explicit_dependencies.empty())
            {
                ImGui::Separator();
                ImGui::TextUnformatted("Invalid Explicit Edges");
                for (const auto& invalid_edge : diagnostics.invalid_explicit_dependencies)
                {
                    const auto from = invalid_edge.first;
                    const auto to = invalid_edge.second;
                    if (from.IsValid())
                    {
                        ImGui::BulletText("%u -> %u", from.value, to.value);
                    }
                    else
                    {
                        ImGui::BulletText("<INVALID> -> %u", to.value);
                    }
                }
            }

            if (!diagnostics.cycle_nodes.empty())
            {
                ImGui::Separator();
                ImGui::TextUnformatted("Cycle Nodes");
                for (const auto cycle_node : diagnostics.cycle_nodes)
                {
                    ImGui::BulletText("%u", cycle_node.value);
                }
            }
        }

        ImGui::Separator();
    }

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
    device.swapchain_resize_policy = GetDefaultSwapchainResizePolicy(device.type);
    m_render_device_type = device.type;
    m_swapchain_resize_policy_ui = device.swapchain_resize_policy;
    m_swapchain_resize_policy_ui_initialized = true;

    m_resource_manager = std::make_shared<RendererInterface::ResourceOperator>(device);
    m_last_render_width = m_resource_manager->GetCurrentRenderWidth();
    m_last_render_height = m_resource_manager->GetCurrentRenderHeight();

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
