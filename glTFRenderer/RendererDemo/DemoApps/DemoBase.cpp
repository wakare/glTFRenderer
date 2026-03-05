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
    if (m_rhi_switch_requested)
    {
        if (!m_rhi_switch_callback_installed && m_window)
        {
            m_rhi_switch_callback_installed = true;
            m_window->RegisterTickCallback([this](unsigned long long interval)
            {
                TickPendingRHISwitch(interval);
            });
        }
        return;
    }

    if (m_rhi_switch_in_progress)
    {
        return;
    }

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
            {
                const char* runtime_rhi_options[] = {"DX12", "Vulkan"};
                int runtime_rhi_selection = m_runtime_rhi_ui_selection == RendererInterface::DX12 ? 0 : 1;
                if (ImGui::Combo("Runtime RHI", &runtime_rhi_selection, runtime_rhi_options, IM_ARRAYSIZE(runtime_rhi_options)))
                {
                    m_runtime_rhi_ui_selection = runtime_rhi_selection == 0 ? RendererInterface::DX12 : RendererInterface::VULKAN;
                }

                if (m_rhi_switch_in_progress)
                {
                    ImGui::TextUnformatted("RHI switch in progress...");
                }
                else if (m_rhi_switch_requested)
                {
                    ImGui::TextUnformatted("RHI switch queued for next tick...");
                }

                const bool can_request_switch = !m_rhi_switch_requested &&
                                                !m_rhi_switch_in_progress &&
                                                m_runtime_rhi_ui_selection != m_render_device_type;
                if (!can_request_switch)
                {
                    ImGui::BeginDisabled();
                }
                if (ImGui::Button("Apply Runtime RHI Switch"))
                {
                    RequestRuntimeRHISwitch(m_runtime_rhi_ui_selection);
                }
                if (!can_request_switch)
                {
                    ImGui::EndDisabled();
                }
                if (!m_rhi_switch_last_error.empty())
                {
                    ImGui::TextWrapped("Last RHI switch error: %s", m_rhi_switch_last_error.c_str());
                }
            }
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
            ImGui::Text("Auto-Pruned Unmapped Bindings: %u across %u node(s)",
                diagnostics.auto_pruned_binding_count,
                diagnostics.auto_pruned_node_count);
            ImGui::Text("Invalid Explicit Edges: %u", static_cast<unsigned>(diagnostics.invalid_explicit_dependencies.size()));
            ImGui::Text("Cycle Nodes: %u", static_cast<unsigned>(diagnostics.cycle_nodes.size()));
            ImGui::Text("Execution Signature: %zu", diagnostics.execution_signature);
            ImGui::Text("Cached Nodes: %zu | Cached Order Size: %zu",
                diagnostics.cached_execution_node_count,
                diagnostics.cached_execution_order_size);

            if (!diagnostics.auto_pruned_nodes.empty())
            {
                ImGui::Separator();
                ImGui::TextUnformatted("Auto-Pruned Nodes");
                for (const auto& pruned_node : diagnostics.auto_pruned_nodes)
                {
                    ImGui::BulletText("[%u] %s / %s | buffer=%u texture=%u rt_texture=%u",
                        pruned_node.node_handle.value,
                        pruned_node.group_name.c_str(),
                        pruned_node.pass_name.c_str(),
                        pruned_node.buffer_count,
                        pruned_node.texture_count,
                        pruned_node.render_target_texture_count);
                }
            }

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
            ImGui::Text("RenderGraph CPU: %.3f ms | GPU: %.3f ms | Passes: %u (Exec %u / Skip %u, Validation %u) | Frame: %llu",
                frame_stats.cpu_total_ms,
                frame_stats.gpu_total_ms,
                static_cast<unsigned>(frame_stats.pass_stats.size()),
                frame_stats.executed_pass_count,
                frame_stats.skipped_pass_count,
                frame_stats.skipped_validation_pass_count,
                frame_stats.frame_index);
        }
        else
        {
            ImGui::Text("RenderGraph CPU: %.3f ms | GPU: N/A | Passes: %u (Exec %u / Skip %u, Validation %u) | Frame: %llu",
                frame_stats.cpu_total_ms,
                static_cast<unsigned>(frame_stats.pass_stats.size()),
                frame_stats.executed_pass_count,
                frame_stats.skipped_pass_count,
                frame_stats.skipped_validation_pass_count,
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
            if (ImGui::BeginTable("RenderPassTimingTable", 5, table_flags, ImVec2(0.0f, 220.0f)))
            {
                ImGui::TableSetupColumn("System");
                ImGui::TableSetupColumn("Pass");
                ImGui::TableSetupColumn("State");
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
                    if (pass_stat.executed)
                    {
                        ImGui::TextUnformatted("Executed");
                    }
                    else if (pass_stat.skipped_due_to_validation)
                    {
                        ImGui::TextUnformatted("Skipped(Validation)");
                    }
                    else
                    {
                        ImGui::TextUnformatted("Skipped");
                    }
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%.3f", pass_stat.cpu_time_ms);
                    ImGui::TableSetColumnIndex(4);
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
    m_launch_arguments = arguments;
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

    if (m_render_graph)
    {
        m_render_graph->CompileRenderPassAndExecute();
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
    m_pending_render_device_type = device.type;
    m_runtime_rhi_ui_selection = device.type;
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

bool DemoBase::RequestRuntimeRHISwitch(RendererInterface::RenderDeviceType new_device_type)
{
    if (m_rhi_switch_in_progress || m_rhi_switch_requested)
    {
        return false;
    }
    if (new_device_type == m_render_device_type)
    {
        return false;
    }

    m_pending_render_device_type = new_device_type;
    m_runtime_rhi_ui_selection = new_device_type;
    m_rhi_switch_last_error.clear();
    m_rhi_switch_requested = true;
    return true;
}

void DemoBase::TickPendingRHISwitch(unsigned long long time_interval)
{
    (void)time_interval;
    if (!m_rhi_switch_requested)
    {
        m_rhi_switch_callback_installed = false;
        if (m_render_graph)
        {
            m_render_graph->CompileRenderPassAndExecute();
        }
        return;
    }
    if (m_rhi_switch_in_progress)
    {
        return;
    }
    ExecutePendingRHISwitch();
}

bool DemoBase::ExecutePendingRHISwitch()
{
    if (!m_rhi_switch_requested)
    {
        return true;
    }

    if (!m_window || !m_resource_manager || !m_render_graph)
    {
        m_rhi_switch_requested = false;
        m_rhi_switch_callback_installed = false;
        m_rhi_switch_last_error = "Missing runtime render context.";
        return false;
    }

    if (m_pending_render_device_type == m_render_device_type)
    {
        m_rhi_switch_requested = false;
        m_rhi_switch_callback_installed = false;
        m_rhi_switch_last_error.clear();
        m_render_graph->CompileRenderPassAndExecute();
        return true;
    }

    m_rhi_switch_in_progress = true;
    m_rhi_switch_last_error.clear();
    const auto non_render_state_snapshot = CaptureNonRenderStateSnapshot();

    const bool previous_per_frame_resource_binding = m_resource_manager->IsPerFrameResourceBindingEnabled();
    unsigned previous_back_buffer_count = m_resource_manager->GetBackBufferCount();
    if (previous_back_buffer_count == 0)
    {
        previous_back_buffer_count = 3;
    }
    const auto previous_swapchain_policy = m_resource_manager->GetSwapchainResizePolicy();
    const auto previous_validation_policy = m_render_graph->GetValidationPolicy();

    if (!RendererInterface::CleanupRenderRuntimeContext(m_render_graph, m_resource_manager, false))
    {
        m_rhi_switch_last_error = "Failed to cleanup runtime resources before RHI recreation.";
        m_rhi_switch_requested = false;
        m_rhi_switch_in_progress = false;
        m_rhi_switch_callback_installed = false;
        return false;
    }
    m_render_target_desc_infos.clear();

    RendererInterface::RenderDeviceDesc device{};
    device.window = m_window->GetHandle();
    device.type = m_pending_render_device_type;
    device.back_buffer_count = previous_back_buffer_count;
    device.swapchain_resize_policy = previous_swapchain_policy;

    m_resource_manager = std::make_shared<RendererInterface::ResourceOperator>(device);
    m_resource_manager->SetPerFrameResourceBindingEnabled(previous_per_frame_resource_binding);
    m_swapchain_resize_policy_ui = previous_swapchain_policy;
    m_swapchain_resize_policy_ui_initialized = true;

    m_render_graph = std::make_shared<RendererInterface::RenderGraph>(*m_resource_manager, *m_window);
    m_render_graph->SetValidationPolicy(previous_validation_policy);
    m_render_graph->RegisterTickCallback([this](unsigned long long time){ TickFrame(time); });
    m_render_graph->RegisterDebugUICallback([this]() { DrawDebugUI(); });

    if (!ReinitializeAfterRHIRecreate())
    {
        m_rhi_switch_last_error = "Failed to rebuild demo resources after RHI recreation.";
        m_rhi_switch_requested = false;
        m_rhi_switch_in_progress = false;
        m_rhi_switch_callback_installed = false;
        if (m_render_graph)
        {
            m_render_graph->RegisterTickCallback([this](unsigned long long time){ TickFrame(time); });
            m_render_graph->RegisterDebugUICallback([this]() { DrawDebugUI(); });
            m_render_graph->CompileRenderPassAndExecute();
        }
        return false;
    }

    if (!ApplyNonRenderStateSnapshot(non_render_state_snapshot))
    {
        m_rhi_switch_last_error = "RHI switched, but failed to restore non-render state snapshot.";
    }

    m_render_graph->CompileRenderPassAndExecute();
    m_render_device_type = device.type;
    m_pending_render_device_type = m_render_device_type;
    m_runtime_rhi_ui_selection = m_render_device_type;
    m_last_render_width = m_resource_manager->GetCurrentRenderWidth();
    m_last_render_height = m_resource_manager->GetCurrentRenderHeight();

    m_rhi_switch_requested = false;
    m_rhi_switch_in_progress = false;
    m_rhi_switch_callback_installed = false;
    return true;
}

bool DemoBase::ReinitializeAfterRHIRecreate()
{
    if (!m_resource_manager || !m_render_graph)
    {
        return false;
    }

    RETURN_IF_FALSE(RebuildRenderRuntimeObjects())

    for (const auto& module : m_modules)
    {
        if (!module)
        {
            continue;
        }
        module->FinalizeModule(*m_resource_manager);
    }

    for (const auto& system : m_systems)
    {
        if (!system)
        {
            continue;
        }
        system->FinalizeModule(*m_resource_manager);
        system->Init(*m_resource_manager, *m_render_graph);
    }

    return true;
}

bool DemoBase::RebuildRenderRuntimeObjects()
{
    if (!m_modules.empty() || !m_systems.empty())
    {
        return true;
    }
    return InitInternal(m_launch_arguments);
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
    render_target_desc.size_mode = RendererInterface::RenderTargetSizeMode::FIXED;
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
