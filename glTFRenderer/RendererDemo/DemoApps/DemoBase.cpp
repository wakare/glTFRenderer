#include "DemoBase.h"

#include "DemoRegistry.h"
#include "RendererInterface.h"
#include "RendererSystem/RendererSystemBase.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <imgui/imgui.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <string_view>
#include <unordered_map>

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

    unsigned GetDefaultBackBufferCount(
        RendererInterface::RenderDeviceType type,
        RendererInterface::SwapchainPresentMode present_mode)
    {
        if (type == RendererInterface::DX12 && present_mode == RendererInterface::SwapchainPresentMode::MAILBOX)
        {
            return 4;
        }

        return 3;
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

    const char* ToString(RendererInterface::SwapchainPresentMode mode)
    {
        switch (mode)
        {
        case RendererInterface::SwapchainPresentMode::VSYNC:
            return "VSync";
        case RendererInterface::SwapchainPresentMode::MAILBOX:
            return "Mailbox";
        default:
            return "Unknown";
        }
    }

    std::string SanitizeFileName(std::string value)
    {
        for (char& ch : value)
        {
            if ((ch >= 'a' && ch <= 'z') ||
                (ch >= 'A' && ch <= 'Z') ||
                (ch >= '0' && ch <= '9') ||
                ch == '-' || ch == '_')
            {
                continue;
            }
            ch = '_';
        }

        if (value.empty())
        {
            value = "snapshot";
        }
        return value;
    }

    std::string BuildTimestampString()
    {
        const auto now = std::chrono::system_clock::now();
        const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm local_tm{};
        localtime_s(&local_tm, &now_time);
        std::ostringstream stamp_stream;
        stamp_stream << std::put_time(&local_tm, "%Y%m%d_%H%M%S");
        return stamp_stream.str();
    }

    bool IsRuntimeRelaunchArgument(std::string_view argument)
    {
        return argument == "-dx" ||
               argument == "-dx12" ||
               argument == "-vk" ||
               argument == "-vulkan" ||
               argument == "-mailbox" ||
               argument == "-novsync" ||
               argument == "-vsync" ||
               argument == "-disable-debug-ui";
    }

    bool IsSnapshotLaunchArgument(std::string_view argument)
    {
        constexpr std::string_view k_snapshot_prefix = "-snapshot=";
        return argument.rfind(k_snapshot_prefix, 0) == 0;
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

    const auto tick_other_begin = std::chrono::steady_clock::now();
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
    PollPendingRenderDocCapture();
    const auto tick_other_end = std::chrono::steady_clock::now();

    if (m_resource_manager && m_render_graph)
    {
        const auto module_tick_begin = std::chrono::steady_clock::now();
        for (const auto& module : m_modules)
        {
            module->Tick(*m_resource_manager, time_interval);
        }
        const auto module_tick_end = std::chrono::steady_clock::now();

        const auto system_tick_begin = std::chrono::steady_clock::now();
        for (auto& system : m_systems)
        {
            system->Tick(*m_resource_manager, *m_render_graph, time_interval);
        }
        const auto system_tick_end = std::chrono::steady_clock::now();

        const auto to_ms = [](const auto& begin, const auto& end) -> float
        {
            return std::chrono::duration<float, std::milli>(end - begin).count();
        };
        m_render_graph->SetTickCallbackBreakdown(
            to_ms(tick_other_begin, tick_other_end),
            to_ms(module_tick_begin, module_tick_end),
            to_ms(system_tick_begin, system_tick_end));
    }
    else if (m_render_graph)
    {
        const auto to_ms = [](const auto& begin, const auto& end) -> float
        {
            return std::chrono::duration<float, std::milli>(end - begin).count();
        };
        m_render_graph->SetTickCallbackBreakdown(
            to_ms(tick_other_begin, tick_other_end),
            0.0f,
            0.0f);
    }
}

void DemoBase::TickFrameInternal(unsigned long long time_interval)
{
}

void DemoBase::DrawDebugUI()
{
    const bool panel_visible = ImGui::Begin(GetDemoPanelName());
    if (!panel_visible)
    {
        ImGui::End();
        return;
    }

    const auto& demo_descriptors = DemoRegistry::GetDemoDescriptors();
    const DemoRegistry::DemoDescriptor* current_demo_descriptor =
        DemoRegistry::FindDemoDescriptorByCommandName(GetDemoCommandName());
    const char* current_demo_display_name =
        current_demo_descriptor ? current_demo_descriptor->display_name : GetDemoCommandName();

    if (HasPendingDemoSwitch())
    {
        ImGui::Text("Pending Demo Switch: %s", m_pending_demo_switch_name.c_str());
    }

    if (ImGui::BeginCombo("Demo", current_demo_display_name))
    {
        for (const DemoRegistry::DemoDescriptor& descriptor : demo_descriptors)
        {
            const bool selected = current_demo_descriptor == &descriptor;
            if (ImGui::Selectable(descriptor.display_name, selected) && !selected)
            {
                RequestDemoSwitch(descriptor.command_name);
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("Frame %.3f ms (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::Separator();

    if (ImGui::CollapsingHeader("State / Snapshot"))
    {
        if (ImGui::Button("Record Snapshot"))
        {
            RecordCurrentStateSnapshot();
        }

        const bool has_recorded_snapshot = static_cast<bool>(m_recorded_state_snapshot);

        ImGui::SameLine();
        if (!has_recorded_snapshot)
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Restore Snapshot"))
        {
            RestoreRecordedStateSnapshot();
        }
        if (!has_recorded_snapshot)
        {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (!has_recorded_snapshot)
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Clear Snapshot"))
        {
            ClearRecordedStateSnapshot();
        }
        if (!has_recorded_snapshot)
        {
            ImGui::EndDisabled();
        }

        ImGui::Text("Recorded Snapshot: %s", has_recorded_snapshot ? "Available" : "None");
        if (!m_recorded_state_snapshot_status.empty())
        {
            ImGui::TextWrapped("Snapshot Status: %s", m_recorded_state_snapshot_status.c_str());
        }

        ImGui::Separator();
        ImGui::InputText("Export Snapshot Name", m_snapshot_export_name, IM_ARRAYSIZE(m_snapshot_export_name));
        if (ImGui::Button("Export Current Snapshot JSON"))
        {
            ExportCurrentStateSnapshotToJson();
        }
        if (!m_last_snapshot_export_path.empty())
        {
            ImGui::Text("Last Exported Snapshot: %s", m_last_snapshot_export_path.c_str());
        }

        ImGui::Separator();
        ImGui::InputText("Snapshot Dir", m_snapshot_directory, IM_ARRAYSIZE(m_snapshot_directory));
        ImGui::SameLine();
        if (ImGui::Button("Refresh Snapshot JSONs"))
        {
            RefreshImportableStateSnapshotList();
        }

        if (ImGui::BeginListBox("Local Snapshot JSONs", ImVec2(0.0f, 120.0f)))
        {
            for (int index = 0; index < static_cast<int>(m_snapshot_file_entries.size()); ++index)
            {
                const auto& entry = m_snapshot_file_entries[static_cast<size_t>(index)];
                const bool selected = index == m_snapshot_selected_index;
                if (ImGui::Selectable(entry.file_name.c_str(), selected))
                {
                    m_snapshot_selected_index = index;
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndListBox();
        }

        if (ImGui::Button("Import Selected Snapshot"))
        {
            if (m_snapshot_selected_index >= 0 &&
                m_snapshot_selected_index < static_cast<int>(m_snapshot_file_entries.size()))
            {
                const auto& entry = m_snapshot_file_entries[static_cast<size_t>(m_snapshot_selected_index)];
                ImportStateSnapshotFromJson(entry.full_path);
            }
            else
            {
                m_snapshot_io_status = "No local snapshot JSON is selected.";
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete Selected Snapshot JSON"))
        {
            DeleteSelectedStateSnapshotJson();
        }

        if (!m_last_snapshot_import_path.empty())
        {
            ImGui::Text("Last Imported Snapshot: %s", m_last_snapshot_import_path.c_str());
        }
        if (!m_snapshot_io_status.empty())
        {
            ImGui::TextWrapped("Snapshot File Status: %s", m_snapshot_io_status.c_str());
        }

        ImGui::Separator();
    }

    if (ImGui::CollapsingHeader("Runtime / Diagnostics"))
    {
        if (m_render_graph && ImGui::CollapsingHeader("Framework Controls"))
        {
            m_render_graph->DrawFrameworkDebugUI();
        }

        if (m_render_graph && ImGui::CollapsingHeader("RenderDoc"))
        {
            ImGui::Text("Capture Enabled: %s", m_render_graph->IsRenderDocCaptureEnabled() ? "Yes" : "No");
            ImGui::Text("Capture Available: %s", m_render_graph->IsRenderDocCaptureAvailable() ? "Yes" : "No");
            if (!m_renderdoc_startup_status.empty())
            {
                ImGui::TextWrapped("Startup Status: %s", m_renderdoc_startup_status.c_str());
            }

            ImGui::Checkbox("Auto Open Replay UI After Capture", &m_renderdoc_auto_open_after_capture);
            ImGui::InputText("Capture Name", m_renderdoc_capture_name, IM_ARRAYSIZE(m_renderdoc_capture_name));
            ImGui::InputText("Capture Dir", m_renderdoc_capture_directory, IM_ARRAYSIZE(m_renderdoc_capture_directory));

            const bool disable_capture_button = m_renderdoc_manual_capture_pending;
            if (disable_capture_button)
            {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Capture Current Frame"))
            {
                QueueManualRenderDocCapture(m_renderdoc_auto_open_after_capture);
            }
            if (disable_capture_button)
            {
                ImGui::EndDisabled();
            }

            const bool has_last_capture =
                m_render_graph->WasLastRenderDocCaptureSuccessful() &&
                !m_render_graph->GetLastRenderDocCapturePath().empty();
            ImGui::SameLine();
            const bool disable_open_last_capture = !has_last_capture;
            if (disable_open_last_capture)
            {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Open Last Capture In RenderDoc"))
            {
                OpenLastRenderDocCaptureInReplayUI();
            }
            if (disable_open_last_capture)
            {
                ImGui::EndDisabled();
            }

            if (m_renderdoc_manual_capture_pending)
            {
                ImGui::Text(
                    "Pending Capture Frame: %llu",
                    static_cast<unsigned long long>(m_renderdoc_manual_capture_frame_index));
            }

            if (has_last_capture)
            {
                ImGui::TextWrapped("Last Capture: %s", m_render_graph->GetLastRenderDocCapturePath().c_str());
            }
            if (!m_renderdoc_ui_status.empty())
            {
                ImGui::TextWrapped("RenderDoc Status: %s", m_renderdoc_ui_status.c_str());
            }
        }

        if (m_resource_manager && ImGui::CollapsingHeader("Surface / Presentation"))
        {
            if (!m_swapchain_resize_policy_ui_initialized)
            {
                m_swapchain_resize_policy_ui = m_resource_manager->GetSwapchainResizePolicy();
                m_swapchain_resize_policy_ui_initialized = true;
            }
            if (!m_swapchain_present_mode_ui_initialized)
            {
                m_swapchain_present_mode_ui = m_resource_manager->GetSwapchainPresentMode();
                m_swapchain_present_mode_ui_initialized = true;
            }

            ImGui::Text("API: %s", ToString(m_render_device_type));
            ImGui::Text("Present Mode: %s", ToString(m_resource_manager->GetSwapchainPresentMode()));
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
            int present_mode_selection =
                m_swapchain_present_mode_ui == RendererInterface::SwapchainPresentMode::VSYNC ? 0 : 1;
            const char* present_mode_options[] = {"VSync", "Mailbox"};
            if (ImGui::Combo("Swapchain Present Mode", &present_mode_selection, present_mode_options, IM_ARRAYSIZE(present_mode_options)))
            {
                const auto new_mode =
                    present_mode_selection == 0
                        ? RendererInterface::SwapchainPresentMode::VSYNC
                        : RendererInterface::SwapchainPresentMode::MAILBOX;
                m_swapchain_present_mode_ui = new_mode;
                m_resource_manager->SetSwapchainPresentMode(new_mode);
                m_resource_manager->InvalidateSwapchainResizeRequest();
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

        if (m_render_graph && ImGui::CollapsingHeader("RenderGraph / Validation"))
        {
            const auto& diagnostics = m_render_graph->GetDependencyDiagnostics();
            const ImGuiTableFlags summary_table_flags =
                ImGuiTableFlags_BordersInnerV |
                ImGuiTableFlags_SizingStretchProp;
            if (ImGui::BeginTable("RenderGraphValidationSummary", 2, summary_table_flags))
            {
                ImGui::TableSetupColumn("Metric");
                ImGui::TableSetupColumn("Value");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Graph Valid");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(diagnostics.graph_valid ? "Yes" : "No");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Auto-Merged Inferred Edges");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", diagnostics.auto_merged_dependency_count);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Auto-Pruned Bindings");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text(
                    "%u across %u node(s)",
                    diagnostics.auto_pruned_binding_count,
                    diagnostics.auto_pruned_node_count);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Invalid Explicit Edges");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", static_cast<unsigned>(diagnostics.invalid_explicit_dependencies.size()));

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Cycle Nodes");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", static_cast<unsigned>(diagnostics.cycle_nodes.size()));

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Execution Signature");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%zu", diagnostics.execution_signature);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Cached Plan");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text(
                    "%zu nodes | %zu order",
                    diagnostics.cached_execution_node_count,
                    diagnostics.cached_execution_order_size);

                ImGui::EndTable();
            }

            if (!diagnostics.auto_pruned_nodes.empty() && ImGui::TreeNode("Auto-Pruned Nodes"))
            {
                const ImGuiTableFlags detail_table_flags =
                    ImGuiTableFlags_Borders |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_SizingStretchProp |
                    ImGuiTableFlags_ScrollY;
                if (ImGui::BeginTable("AutoPrunedNodeTable", 6, detail_table_flags, ImVec2(0.0f, 180.0f)))
                {
                    ImGui::TableSetupColumn("Node");
                    ImGui::TableSetupColumn("Group");
                    ImGui::TableSetupColumn("Pass");
                    ImGui::TableSetupColumn("Buffer");
                    ImGui::TableSetupColumn("Texture");
                    ImGui::TableSetupColumn("RT Texture");
                    ImGui::TableHeadersRow();

                    for (const auto& pruned_node : diagnostics.auto_pruned_nodes)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%u", pruned_node.node_handle.value);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(pruned_node.group_name.c_str());
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextUnformatted(pruned_node.pass_name.c_str());
                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text("%u", pruned_node.buffer_count);
                        ImGui::TableSetColumnIndex(4);
                        ImGui::Text("%u", pruned_node.texture_count);
                        ImGui::TableSetColumnIndex(5);
                        ImGui::Text("%u", pruned_node.render_target_texture_count);
                    }
                    ImGui::EndTable();
                }
                ImGui::TreePop();
            }

            if (!diagnostics.invalid_explicit_dependencies.empty() && ImGui::TreeNode("Invalid Explicit Edges"))
            {
                const ImGuiTableFlags edge_table_flags =
                    ImGuiTableFlags_Borders |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_SizingStretchProp;
                if (ImGui::BeginTable("InvalidExplicitEdgeTable", 2, edge_table_flags))
                {
                    ImGui::TableSetupColumn("From");
                    ImGui::TableSetupColumn("To");
                    ImGui::TableHeadersRow();

                    for (const auto& invalid_edge : diagnostics.invalid_explicit_dependencies)
                    {
                        const auto from = invalid_edge.first;
                        const auto to = invalid_edge.second;
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        if (from.IsValid())
                        {
                            ImGui::Text("%u", from.value);
                        }
                        else
                        {
                            ImGui::TextUnformatted("<INVALID>");
                        }
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%u", to.value);
                    }
                    ImGui::EndTable();
                }
                ImGui::TreePop();
            }

            if (!diagnostics.cycle_nodes.empty() && ImGui::TreeNode("Cycle Nodes"))
            {
                const ImGuiTableFlags cycle_table_flags =
                    ImGuiTableFlags_Borders |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_SizingStretchProp;
                if (ImGui::BeginTable("CycleNodeTable", 1, cycle_table_flags))
                {
                    ImGui::TableSetupColumn("Node");
                    ImGui::TableHeadersRow();

                    for (const auto cycle_node : diagnostics.cycle_nodes)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%u", cycle_node.value);
                    }
                    ImGui::EndTable();
                }
                ImGui::TreePop();
            }
        }

        ImGui::Separator();
    }

    if (m_render_graph && ImGui::CollapsingHeader("Performance / Timing"))
    {
        const auto& frame_stats = m_render_graph->GetLastFrameStats();
        const auto& frame_timing = m_render_graph->GetLastFrameTimingBreakdown();
        const auto to_pass_type_name = [](RendererInterface::RenderPassType pass_type) -> const char*
        {
            switch (pass_type)
            {
            case RendererInterface::RenderPassType::GRAPHICS:
                return "Graphics";
            case RendererInterface::RenderPassType::COMPUTE:
                return "Compute";
            case RendererInterface::RenderPassType::RAY_TRACING:
                return "RayTracing";
            }
            return "Unknown";
        };
        if (frame_stats.gpu_time_valid)
        {
            ImGui::Text("RenderGraph CPU: %.3f ms | GPU: %.3f ms | Frame: %llu",
                frame_stats.cpu_total_ms,
                frame_stats.gpu_total_ms,
                frame_stats.frame_index);
        }
        else
        {
            ImGui::Text("RenderGraph CPU: %.3f ms | GPU: N/A | Frame: %llu",
                frame_stats.cpu_total_ms,
                frame_stats.frame_index);
        }
        ImGui::Text("Pass Count: Total %u | Exec %u | Skip %u (Validation %u / Missing %u)",
            frame_stats.total_pass_count,
            frame_stats.executed_pass_count,
            frame_stats.skipped_pass_count,
            frame_stats.skipped_validation_pass_count,
            frame_stats.skipped_missing_pass_count);
        ImGui::Text("CPU Split: Exec %.3f ms | Skip %.3f ms (Validation %.3f ms)",
            frame_stats.cpu_executed_ms,
            frame_stats.cpu_skipped_ms,
            frame_stats.cpu_skipped_validation_ms);
        ImGui::Text("Pass Type Count: Gfx %u (Exec %u) | Compute %u (Exec %u) | RT %u (Exec %u)",
            frame_stats.graphics_pass_count,
            frame_stats.executed_graphics_pass_count,
            frame_stats.compute_pass_count,
            frame_stats.executed_compute_pass_count,
            frame_stats.ray_tracing_pass_count,
            frame_stats.executed_ray_tracing_pass_count);
        if (m_window)
        {
            const auto window_loop_timing = m_window->GetLastLoopTiming();
            const int window_refresh_hz = m_window->GetWindowRefreshRate();
            if (window_refresh_hz > 0)
            {
                ImGui::Text("Window Refresh: %d Hz", window_refresh_hz);
            }
            else
            {
                ImGui::TextUnformatted("Window Refresh: Unknown");
            }
            if (window_loop_timing.valid)
            {
                ImGui::Text(
                    "Window Loop: Wall %.3f ms | ThreadCPU %.3f ms | Idle/Wait %.3f ms",
                    window_loop_timing.loop_total_ms,
                    window_loop_timing.loop_thread_cpu_ms,
                    window_loop_timing.idle_wait_ms);
                ImGui::Text(
                    "Window Loop Detail: TickCallback %.3f ms (CPU %.3f) | PollEvents %.3f ms (CPU %.3f) | Other %.3f ms",
                    window_loop_timing.tick_callback_ms,
                    window_loop_timing.tick_callback_thread_cpu_ms,
                    window_loop_timing.poll_events_ms,
                    window_loop_timing.poll_events_thread_cpu_ms,
                    window_loop_timing.non_tick_ms);
            }
        }
        if (frame_timing.valid)
        {
            ImGui::Text("Frame CPU: %.3f ms | Pass CPU: %.3f ms | Non-Pass CPU: %.3f ms | Untracked: %.3f ms",
                frame_timing.frame_total_ms,
                frame_timing.execute_passes_ms,
                frame_timing.non_pass_cpu_ms,
                frame_timing.untracked_ms);
            ImGui::Text("CPU Wait Estimate: %.3f ms [WaitPrev %.3f | AcquireCmd %.3f | AcquireSwapchain %.3f | Submit %.3f | PresentCall %.3f]",
                frame_timing.frame_wait_total_ms,
                frame_timing.wait_previous_frame_ms,
                frame_timing.acquire_command_list_ms,
                frame_timing.acquire_swapchain_ms,
                frame_timing.submit_command_list_ms,
                frame_timing.present_call_ms);
            ImGui::Text("Prepare: %.3f ms [Sync %.3f | Tick/UI %.3f | WaitPrev %.3f | DeferredRelease %.3f | AcquireCtx %.3f (ResolveProfiler %.3f / AcquireCmd %.3f / AcquireSwapchain %.3f)]",
                frame_timing.prepare_frame_ms,
                frame_timing.sync_window_surface_ms,
                frame_timing.tick_and_debug_ui_build_ms,
                frame_timing.wait_previous_frame_ms,
                frame_timing.deferred_release_ms,
                frame_timing.acquire_context_ms,
                frame_timing.resolve_gpu_profiler_ms,
                frame_timing.acquire_command_list_ms,
                frame_timing.acquire_swapchain_ms);
            ImGui::Text("Tick Breakdown: Tick %.3f ms [Other/Resize %.3f | Modules %.3f | Systems %.3f] | BuildUI %.3f ms",
                frame_timing.tick_callback_ms,
                frame_timing.tick_other_ms,
                frame_timing.module_tick_ms,
                frame_timing.system_tick_ms,
                frame_timing.debug_ui_build_ms);
            ImGui::Text("Execute: %.3f ms [Plan+Diagnostics %.3f | Passes %.3f | DescriptorGC %.3f] | Blit %.3f ms",
                frame_timing.execute_render_graph_ms,
                frame_timing.execution_planning_ms,
                frame_timing.execute_passes_ms,
                frame_timing.collect_unused_descriptor_ms,
                frame_timing.blit_to_swapchain_ms);
            ImGui::Text("Finalize: %.3f ms [RenderDebugUI %.3f | Present %.3f (Submit %.3f / PresentCall %.3f)]",
                frame_timing.finalize_submission_ms,
                frame_timing.render_debug_ui_ms,
                frame_timing.present_ms,
                frame_timing.submit_command_list_ms,
                frame_timing.present_call_ms);
        }
        else
        {
            ImGui::TextUnformatted("Frame CPU breakdown: unavailable this tick (frame not rendered or surface invalid).");
        }

        if (!frame_stats.pass_stats.empty())
        {
            if (ImGui::TreeNode("Per-group Timing Summary"))
            {
                struct GroupTimingAggregate
                {
                    float cpu_total_ms{0.0f};
                    float gpu_total_ms{0.0f};
                    bool gpu_time_valid{false};
                    unsigned total_count{0};
                    unsigned executed_count{0};
                    unsigned skipped_count{0};
                };
                std::unordered_map<std::string_view, GroupTimingAggregate> group_aggregates;
                group_aggregates.reserve(frame_stats.pass_stats.size());
                std::vector<std::string_view> group_order;
                group_order.reserve(frame_stats.pass_stats.size());
                for (const auto& pass_stat : frame_stats.pass_stats)
                {
                    const std::string_view group_name = pass_stat.group_name;
                    auto [aggregate_it, inserted] = group_aggregates.try_emplace(group_name, GroupTimingAggregate{});
                    if (inserted)
                    {
                        group_order.push_back(group_name);
                    }

                    auto& aggregate = aggregate_it->second;
                    aggregate.cpu_total_ms += pass_stat.cpu_time_ms;
                    ++aggregate.total_count;
                    if (pass_stat.executed)
                    {
                        ++aggregate.executed_count;
                    }
                    else
                    {
                        ++aggregate.skipped_count;
                    }
                    if (pass_stat.gpu_time_valid)
                    {
                        aggregate.gpu_total_ms += pass_stat.gpu_time_ms;
                        aggregate.gpu_time_valid = true;
                    }
                }

                for (const auto group_name : group_order)
                {
                    const auto aggregate_it = group_aggregates.find(group_name);
                    if (aggregate_it == group_aggregates.end())
                    {
                        continue;
                    }

                    const auto& aggregate = aggregate_it->second;
                    if (aggregate.gpu_time_valid)
                    {
                        ImGui::Text("%s: CPU %.3f ms | GPU %.3f ms | Count %u (Exec %u / Skip %u)",
                            group_name.data(),
                            aggregate.cpu_total_ms,
                            aggregate.gpu_total_ms,
                            aggregate.total_count,
                            aggregate.executed_count,
                            aggregate.skipped_count);
                    }
                    else
                    {
                        ImGui::Text("%s: CPU %.3f ms | GPU N/A | Count %u (Exec %u / Skip %u)",
                            group_name.data(),
                            aggregate.cpu_total_ms,
                            aggregate.total_count,
                            aggregate.executed_count,
                            aggregate.skipped_count);
                    }
                }

                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Per-pass Timing Table"))
            {
                const ImGuiTableFlags table_flags =
                    ImGuiTableFlags_Borders |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_SizingStretchProp |
                    ImGuiTableFlags_ScrollY;
                if (ImGui::BeginTable("RenderPassTimingTable", 6, table_flags, ImVec2(0.0f, 220.0f)))
                {
                    ImGui::TableSetupColumn("System");
                    ImGui::TableSetupColumn("Pass");
                    ImGui::TableSetupColumn("Type");
                    ImGui::TableSetupColumn("State");
                    ImGui::TableSetupColumn("CPU ms");
                    ImGui::TableSetupColumn("GPU ms");
                    ImGui::TableHeadersRow();

                    ImGuiListClipper clipper;
                    clipper.Begin(static_cast<int>(frame_stats.pass_stats.size()));
                    while (clipper.Step())
                    {
                        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                        {
                            const auto& pass_stat = frame_stats.pass_stats[static_cast<std::size_t>(row)];
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextUnformatted(pass_stat.group_name.c_str());
                            ImGui::TableSetColumnIndex(1);
                            ImGui::TextUnformatted(pass_stat.pass_name.c_str());
                            ImGui::TableSetColumnIndex(2);
                            ImGui::TextUnformatted(to_pass_type_name(pass_stat.pass_type));
                            ImGui::TableSetColumnIndex(3);
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
                            ImGui::TableSetColumnIndex(4);
                            ImGui::Text("%.3f", pass_stat.cpu_time_ms);
                            ImGui::TableSetColumnIndex(5);
                            if (pass_stat.gpu_time_valid)
                            {
                                ImGui::Text("%.3f", pass_stat.gpu_time_ms);
                            }
                            else
                            {
                                ImGui::TextUnformatted("N/A");
                            }
                        }
                    }
                    ImGui::EndTable();
                }
                ImGui::TreePop();
            }
        }
        else
        {
            ImGui::TextUnformatted("No pass timing data this frame.");
        }

        ImGui::Separator();
    }

    if (ImGui::CollapsingHeader("Feature Controls", ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawDebugUIInternal();

        for (const auto& system : m_systems)
        {
            if (!system)
            {
                continue;
            }

            if (ImGui::CollapsingHeader(system->GetSystemName()))
            {
                system->DrawDebugUI();
            }
        }
    }

    ImGui::End();
}

void DemoBase::PollPendingRenderDocCapture()
{
    if (!m_renderdoc_manual_capture_pending)
    {
        return;
    }

    if (!m_render_graph)
    {
        m_renderdoc_manual_capture_pending = false;
        m_renderdoc_manual_capture_auto_open = false;
        m_renderdoc_manual_capture_frame_index = 0ull;
        m_renderdoc_manual_capture_requested_path.clear();
        m_renderdoc_ui_status = "RenderDoc manual capture failed: render graph is unavailable.";
        return;
    }

    const unsigned long long captured_frame_index = m_render_graph->GetLastRenderDocCaptureFrameIndex();
    if (captured_frame_index == 0ull || captured_frame_index < m_renderdoc_manual_capture_frame_index)
    {
        return;
    }

    const bool capture_success = m_render_graph->WasLastRenderDocCaptureSuccessful();
    const std::string capture_error = m_render_graph->GetLastRenderDocCaptureError();
    const std::string capture_path = m_render_graph->GetLastRenderDocCapturePath();
    if (!capture_success)
    {
        m_renderdoc_ui_status =
            "RenderDoc manual capture failed: " +
            (capture_error.empty() ? std::string("unknown error.") : capture_error);
    }
    else if (captured_frame_index != m_renderdoc_manual_capture_frame_index)
    {
        m_renderdoc_ui_status =
            "RenderDoc manual capture frame mismatch. Expected frame " +
            std::to_string(m_renderdoc_manual_capture_frame_index) +
            ", got " + std::to_string(captured_frame_index) + ".";
    }
    else
    {
        const std::string resolved_capture_path =
            !capture_path.empty() ? capture_path : m_renderdoc_manual_capture_requested_path.string();
        m_renderdoc_ui_status = "RenderDoc manual capture completed: " + resolved_capture_path;

        if (m_renderdoc_manual_capture_auto_open)
        {
            std::string open_status{};
            if (m_render_graph->OpenRenderDocCaptureInReplayUI(std::filesystem::path(resolved_capture_path), open_status))
            {
                m_renderdoc_ui_status += " " + open_status;
            }
            else
            {
                m_renderdoc_ui_status += " Auto-open failed: " + open_status;
            }
        }
    }

    m_renderdoc_manual_capture_pending = false;
    m_renderdoc_manual_capture_auto_open = false;
    m_renderdoc_manual_capture_frame_index = 0ull;
    m_renderdoc_manual_capture_requested_path.clear();
}

bool DemoBase::PreloadRenderDocForDevice(
    RendererInterface::RenderDeviceType device_type,
    bool log_status,
    std::string& out_error)
{
    out_error.clear();

    if (!m_renderdoc_preload_requested)
    {
        m_renderdoc_startup_status =
            "RenderDoc startup preload is disabled. Launch with -renderdoc-ui, -renderdoc-capture, or -renderdoc-required "
            "to enable manual capture before device initialization.";
        return true;
    }

    std::string renderdoc_status{};
    const bool preload_ok =
        RendererInterface::PreloadRenderDocRuntime(device_type, m_renderdoc_required, renderdoc_status);
    if (!renderdoc_status.empty())
    {
        m_renderdoc_startup_status = renderdoc_status;
        if (log_status)
        {
            std::printf("[RenderDoc] %s\n", renderdoc_status.c_str());
        }
    }

    if (!preload_ok && m_renderdoc_required)
    {
        out_error = !renderdoc_status.empty()
            ? renderdoc_status
            : "RenderDoc runtime is required but could not be preloaded before device initialization.";
        return false;
    }

    if (!preload_ok && !renderdoc_status.empty())
    {
        m_renderdoc_ui_status = renderdoc_status;
    }

    return true;
}

bool DemoBase::EnsureRenderDocCaptureConfigured(bool require_available, std::string& out_status)
{
    out_status.clear();
    if (!m_render_graph)
    {
        out_status = "RenderDoc capture is unavailable because the render graph is not initialized.";
        return false;
    }

    if (!m_render_graph->ConfigureRenderDocCapture(true, require_available, out_status))
    {
        return false;
    }

    if (!m_render_graph->IsRenderDocCaptureAvailable())
    {
        if (out_status.empty())
        {
            out_status = "RenderDoc capture is not available for this runtime.";
        }
        return false;
    }

    return true;
}

std::filesystem::path DemoBase::BuildManualRenderDocCapturePath() const
{
    const std::string capture_name = m_renderdoc_capture_name;
    const std::string capture_stem =
        capture_name.empty() ? std::string("capture") : SanitizeFileName(capture_name);
    std::filesystem::path output_dir = std::filesystem::path(m_renderdoc_capture_directory);
    if (output_dir.empty())
    {
        output_dir = std::filesystem::path("build_logs") / "renderdoc";
    }
    return output_dir / (capture_stem + "_" + BuildTimestampString() + ".rdc");
}

bool DemoBase::QueueManualRenderDocCapture(bool auto_open_replay_ui)
{
    if (m_renderdoc_manual_capture_pending)
    {
        m_renderdoc_ui_status = "RenderDoc manual capture is already pending.";
        return false;
    }

    std::string renderdoc_status{};
    if (!EnsureRenderDocCaptureConfigured(false, renderdoc_status))
    {
        m_renderdoc_ui_status = renderdoc_status;
        return false;
    }

    const std::filesystem::path capture_path = BuildManualRenderDocCapturePath();
    std::string request_error{};
    if (!m_render_graph->RequestRenderDocCaptureForCurrentFrame(capture_path, request_error))
    {
        m_renderdoc_ui_status =
            "RenderDoc manual capture request failed: " +
            (request_error.empty() ? std::string("unknown error.") : request_error);
        return false;
    }

    m_renderdoc_manual_capture_pending = true;
    m_renderdoc_manual_capture_auto_open = auto_open_replay_ui;
    m_renderdoc_manual_capture_frame_index = m_render_graph->GetCurrentFrameIndex();
    m_renderdoc_manual_capture_requested_path = capture_path;
    m_renderdoc_ui_status =
        "RenderDoc manual capture armed for frame " +
        std::to_string(m_renderdoc_manual_capture_frame_index) +
        ": " + capture_path.string();
    return true;
}

bool DemoBase::OpenLastRenderDocCaptureInReplayUI()
{
    if (!m_render_graph)
    {
        m_renderdoc_ui_status = "RenderDoc replay launch failed: render graph is unavailable.";
        return false;
    }

    std::string open_status{};
    const bool opened = m_render_graph->OpenLastRenderDocCaptureInReplayUI(open_status);
    m_renderdoc_ui_status = open_status;
    return opened;
}

bool DemoBase::Init(const std::vector<std::string>& arguments)
{
    m_launch_arguments = arguments;
    const std::string default_capture_name = SanitizeFileName(GetDemoCommandName());
    std::snprintf(m_renderdoc_capture_name, sizeof(m_renderdoc_capture_name), "%s", default_capture_name.c_str());
    m_renderdoc_preload_requested = false;
    m_renderdoc_required = false;
    m_renderdoc_manual_capture_pending = false;
    m_renderdoc_manual_capture_auto_open = false;
    m_renderdoc_manual_capture_frame_index = 0ull;
    m_renderdoc_manual_capture_requested_path.clear();
    m_renderdoc_startup_status.clear();
    m_renderdoc_ui_status.clear();
    m_startup_snapshot_path.clear();
    const std::string snapshot_prefix = "-snapshot=";
    for (const auto& argument : arguments)
    {
        if (argument.rfind(snapshot_prefix, 0) == 0)
        {
            m_startup_snapshot_path = argument.substr(snapshot_prefix.length());
        }
    }

    RETURN_IF_FALSE(InitRenderContext(arguments))
    
    RETURN_IF_FALSE(InitInternal(arguments))
    RefreshImportableStateSnapshotList();
    RETURN_IF_FALSE(InitializeRuntimeModulesAndSystems())

    if (!m_startup_snapshot_path.empty())
    {
        RETURN_IF_FALSE(ImportStateSnapshotFromJson(m_startup_snapshot_path))
    }

    StartRenderGraphExecution();
    
    return true;
}

void DemoBase::Run()
{
    m_window->EnterWindowEventLoop();
}

void DemoBase::CleanupWindowBoundRuntimeIfNeeded(bool clear_window_handles)
{
    if (m_window_runtime_cleaned_up)
    {
        return;
    }

    if (m_render_graph || m_resource_manager)
    {
        RendererInterface::CleanupRenderRuntimeContext(m_render_graph, m_resource_manager, clear_window_handles);
    }

    if (m_renderdoc_manual_capture_pending)
    {
        m_renderdoc_ui_status = "RenderDoc manual capture was cancelled because the runtime was recreated or shut down.";
    }
    m_renderdoc_manual_capture_pending = false;
    m_renderdoc_manual_capture_auto_open = false;
    m_renderdoc_manual_capture_frame_index = 0ull;
    m_renderdoc_manual_capture_requested_path.clear();
    m_window_runtime_cleaned_up = true;
    m_rhi_switch_requested = false;
    m_rhi_switch_callback_installed = false;
    m_rhi_switch_in_progress = false;
}

void DemoBase::Shutdown()
{
    CleanupWindowBoundRuntimeIfNeeded(true);

    m_modules.clear();
    m_systems.clear();
    m_render_target_desc_infos.clear();
    if (m_window)
    {
        m_window->RegisterExitCallback({});
    }
    m_window.reset();
    m_rhi_switch_requested = false;
    m_rhi_switch_callback_installed = false;
    m_rhi_switch_in_progress = false;
}

std::vector<std::string> DemoBase::BuildLaunchArgumentsForDemo(const std::string& demo_name) const
{
    std::vector<std::string> launch_arguments{};
    launch_arguments.reserve(m_launch_arguments.size() + 3);
    launch_arguments.push_back(demo_name);

    for (size_t argument_index = 1; argument_index < m_launch_arguments.size(); ++argument_index)
    {
        const std::string_view argument = m_launch_arguments[argument_index];
        if (IsRuntimeRelaunchArgument(argument) || IsSnapshotLaunchArgument(argument))
        {
            continue;
        }
        launch_arguments.push_back(m_launch_arguments[argument_index]);
    }

    launch_arguments.push_back(m_render_device_type == RendererInterface::DX12 ? "-dx12" : "-vulkan");

    const RendererInterface::SwapchainPresentMode present_mode =
        m_resource_manager ? m_resource_manager->GetSwapchainPresentMode() : m_swapchain_present_mode_ui;
    launch_arguments.push_back(
        present_mode == RendererInterface::SwapchainPresentMode::MAILBOX ? "-mailbox" : "-vsync");

    if (!m_debug_ui_enabled)
    {
        launch_arguments.push_back("-disable-debug-ui");
    }

    return launch_arguments;
}

bool DemoBase::InitRenderContext(const std::vector<std::string>& arguments)
{
    bool bUseDX = true;
    RendererInterface::SwapchainPresentMode swapchain_present_mode = RendererInterface::SwapchainPresentMode::VSYNC;
    bool disable_debug_ui = false;
    bool log_renderdoc_status = false;
    
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

        if (argument == "-mailbox" || argument == "-novsync")
        {
            swapchain_present_mode = RendererInterface::SwapchainPresentMode::MAILBOX;
        }

        if (argument == "-vsync")
        {
            swapchain_present_mode = RendererInterface::SwapchainPresentMode::VSYNC;
        }

        if (argument == "-disable-debug-ui")
        {
            disable_debug_ui = true;
        }

        if (argument == "-regression" || argument == "-renderdoc-capture")
        {
            m_renderdoc_preload_requested = true;
            log_renderdoc_status = true;
        }

        if (argument == "-renderdoc-required")
        {
            m_renderdoc_preload_requested = true;
            m_renderdoc_required = true;
            log_renderdoc_status = true;
        }

        if (argument == "-renderdoc-ui")
        {
            m_renderdoc_preload_requested = true;
            log_renderdoc_status = true;
        }
    }

    {
        std::string preload_error{};
        const RendererInterface::RenderDeviceType render_device_type =
            bUseDX ? RendererInterface::DX12 : RendererInterface::VULKAN;
        if (!PreloadRenderDocForDevice(render_device_type, log_renderdoc_status, preload_error))
        {
            return false;
        }
    }

    RendererInterface::RenderWindowDesc window_desc{};
    window_desc.width = m_width;
    window_desc.height = m_height;
    
    m_window = std::make_shared<RendererInterface::RenderWindow>(window_desc);

    RendererInterface::RenderDeviceDesc device{};
    device.window = m_window->GetHandle();
    device.type = bUseDX ? RendererInterface::DX12 : RendererInterface::VULKAN;
    device.back_buffer_count = GetDefaultBackBufferCount(device.type, swapchain_present_mode);
    device.swapchain_resize_policy = GetDefaultSwapchainResizePolicy(device.type);
    device.swapchain_present_mode = swapchain_present_mode;
    device.vulkan_optional_capabilities = GetRequestedVulkanOptionalCapabilities();
    m_debug_ui_enabled = !disable_debug_ui;
    m_render_device_type = device.type;
    m_pending_render_device_type = device.type;
    m_runtime_rhi_ui_selection = device.type;
    m_swapchain_resize_policy_ui = device.swapchain_resize_policy;
    m_swapchain_resize_policy_ui_initialized = true;
    m_swapchain_present_mode_ui = device.swapchain_present_mode;
    m_swapchain_present_mode_ui_initialized = true;

    return CreateRenderRuntimeContext(device, disable_debug_ui);
}

bool DemoBase::CreateRenderRuntimeContext(const RendererInterface::RenderDeviceDesc& device_desc, bool disable_debug_ui)
{
    if (!m_window)
    {
        return false;
    }

    m_window_runtime_cleaned_up = false;
    m_resource_manager = std::make_shared<RendererInterface::ResourceOperator>(device_desc);
    m_last_render_width = m_resource_manager->GetCurrentRenderWidth();
    m_last_render_height = m_resource_manager->GetCurrentRenderHeight();

    m_render_graph = std::make_shared<RendererInterface::RenderGraph>(*m_resource_manager, *m_window, !disable_debug_ui);
    m_render_graph->RegisterTickCallback([this](unsigned long long time){ TickFrame(time); });
    m_render_graph->RegisterDebugUICallback([this]() { DrawDebugUI(); });
    if (disable_debug_ui)
    {
        m_render_graph->EnableDebugUI(false);
    }

    m_window->RegisterExitCallback([this]()
    {
        CleanupWindowBoundRuntimeIfNeeded(true);
    });

    return true;
}

bool DemoBase::InitializeRuntimeModulesAndSystems()
{
    if (!m_resource_manager || !m_render_graph)
    {
        return false;
    }

    for (const auto& module : m_modules)
    {
        if (!module)
        {
            continue;
        }

        if (!module->FinalizeModule(*m_resource_manager))
        {
            return false;
        }
    }

    for (const auto& system : m_systems)
    {
        if (!system)
        {
            continue;
        }

        if (!system->FinalizeModule(*m_resource_manager))
        {
            return false;
        }
        if (!system->Init(*m_resource_manager, *m_render_graph))
        {
            return false;
        }
    }

    return true;
}

void DemoBase::StartRenderGraphExecution()
{
    if (m_render_graph)
    {
        m_render_graph->CompileRenderPassAndExecute();
    }
}

bool DemoBase::RecordCurrentStateSnapshot()
{
    const auto snapshot = CaptureNonRenderStateSnapshot();
    if (!snapshot)
    {
        m_recorded_state_snapshot.reset();
        m_recorded_state_snapshot_status = "Snapshot capture is not implemented for this demo.";
        return false;
    }

    m_recorded_state_snapshot = snapshot;
    m_recorded_state_snapshot_status = "Snapshot recorded.";
    return true;
}

bool DemoBase::RestoreRecordedStateSnapshot()
{
    if (!m_recorded_state_snapshot)
    {
        m_recorded_state_snapshot_status = "No recorded snapshot is available.";
        return false;
    }

    if (!ApplyNonRenderStateSnapshot(m_recorded_state_snapshot))
    {
        m_recorded_state_snapshot_status = "Failed to restore recorded snapshot.";
        return false;
    }

    m_recorded_state_snapshot_status = "Snapshot restored.";
    return true;
}

void DemoBase::ClearRecordedStateSnapshot()
{
    m_recorded_state_snapshot.reset();
    m_recorded_state_snapshot_status = "Snapshot cleared.";
}

bool DemoBase::ExportCurrentStateSnapshotToJson()
{
    const auto snapshot = CaptureNonRenderStateSnapshot();
    if (!snapshot)
    {
        m_snapshot_io_status = "Snapshot export failed: capture is not implemented for this demo.";
        return false;
    }

    nlohmann::json snapshot_json{};
    if (!SerializeNonRenderStateSnapshotToJson(snapshot, snapshot_json))
    {
        m_snapshot_io_status = "Snapshot export failed: serialization is not implemented for this demo.";
        return false;
    }

    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm{};
    localtime_s(&local_tm, &now_time);
    std::ostringstream stamp_stream;
    stamp_stream << std::put_time(&local_tm, "%Y%m%d_%H%M%S");
    const std::string stamp = stamp_stream.str();

    std::filesystem::path output_dir = std::filesystem::path(m_snapshot_directory);
    if (output_dir.empty())
    {
        output_dir = std::filesystem::path("build_logs") / "snapshots";
    }

    std::error_code fs_error;
    std::filesystem::create_directories(output_dir, fs_error);
    if (fs_error)
    {
        m_snapshot_io_status = "Snapshot export failed: unable to create output directory: " + output_dir.string();
        return false;
    }

    const std::string file_stem = SanitizeFileName(m_snapshot_export_name);
    const std::filesystem::path output_path = output_dir / (file_stem + "_" + stamp + ".json");

    nlohmann::json root{};
    root["snapshot_type"] = GetSnapshotTypeName();
    root["schema_version"] = 1;
    root["snapshot"] = std::move(snapshot_json);

    std::ofstream output_stream(output_path, std::ios::out | std::ios::trunc);
    if (!output_stream.is_open())
    {
        m_snapshot_io_status = "Snapshot export failed: unable to open file for write: " + output_path.string();
        return false;
    }

    output_stream << root.dump(2) << "\n";
    output_stream.close();

    m_recorded_state_snapshot = snapshot;
    m_recorded_state_snapshot_status = "Snapshot recorded.";
    m_last_snapshot_export_path = output_path.string();
    m_snapshot_io_status = "Snapshot exported: " + m_last_snapshot_export_path;
    RefreshImportableStateSnapshotList();
    return true;
}

bool DemoBase::ImportStateSnapshotFromJson(const std::filesystem::path& snapshot_path)
{
    std::ifstream input_stream(snapshot_path, std::ios::in);
    if (!input_stream.is_open())
    {
        m_snapshot_io_status = "Snapshot import failed: unable to open file: " + snapshot_path.string();
        return false;
    }

    nlohmann::json root{};
    try
    {
        input_stream >> root;
    }
    catch (const std::exception& exception)
    {
        m_snapshot_io_status = "Snapshot import failed: invalid JSON: " + std::string(exception.what());
        return false;
    }

    if (!root.is_object())
    {
        m_snapshot_io_status = "Snapshot import failed: root JSON must be an object.";
        return false;
    }

    if (root.contains("snapshot_type"))
    {
        if (!root.at("snapshot_type").is_string())
        {
            m_snapshot_io_status = "Snapshot import failed: snapshot_type must be a string.";
            return false;
        }

        const std::string snapshot_type = root.at("snapshot_type").get<std::string>();
        if (snapshot_type != GetSnapshotTypeName())
        {
            m_snapshot_io_status =
                "Snapshot import failed: snapshot type mismatch. Expected '" +
                std::string(GetSnapshotTypeName()) + "', got '" + snapshot_type + "'.";
            return false;
        }
    }

    if (!root.contains("snapshot"))
    {
        m_snapshot_io_status = "Snapshot import failed: missing snapshot object.";
        return false;
    }

    std::string deserialize_error{};
    const auto snapshot = DeserializeNonRenderStateSnapshotFromJson(root.at("snapshot"), deserialize_error);
    if (!snapshot)
    {
        m_snapshot_io_status = "Snapshot import failed: " + deserialize_error;
        return false;
    }

    if (!ApplyNonRenderStateSnapshot(snapshot))
    {
        m_snapshot_io_status = "Snapshot import failed: apply failed.";
        return false;
    }

    m_recorded_state_snapshot = snapshot;
    m_recorded_state_snapshot_status = "Snapshot restored.";
    std::error_code path_error;
    m_last_snapshot_import_path = std::filesystem::absolute(snapshot_path, path_error).string();
    if (path_error)
    {
        m_last_snapshot_import_path = snapshot_path.string();
    }
    m_snapshot_io_status = "Snapshot imported: " + m_last_snapshot_import_path;
    return true;
}

void DemoBase::RefreshImportableStateSnapshotList()
{
    m_snapshot_file_entries.clear();
    m_snapshot_selected_index = -1;

    std::filesystem::path import_dir = std::filesystem::path(m_snapshot_directory);
    if (import_dir.empty())
    {
        m_snapshot_io_status = "Snapshot directory is empty.";
        return;
    }

    std::error_code fs_error;
    if (!std::filesystem::exists(import_dir, fs_error) || fs_error)
    {
        m_snapshot_io_status = "Snapshot directory does not exist: " + import_dir.string();
        return;
    }
    if (!std::filesystem::is_directory(import_dir, fs_error) || fs_error)
    {
        m_snapshot_io_status = "Snapshot path is not a directory: " + import_dir.string();
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(
             import_dir, std::filesystem::directory_options::skip_permission_denied, fs_error))
    {
        if (fs_error)
        {
            break;
        }

        std::error_code entry_error;
        if (!entry.is_regular_file(entry_error) || entry_error)
        {
            continue;
        }

        std::string extension = entry.path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value)
        {
            return static_cast<char>(std::tolower(value));
        });
        if (extension != ".json")
        {
            continue;
        }

        StateSnapshotFileEntry import_entry{};
        import_entry.file_name = entry.path().filename().string();
        import_entry.full_path = std::filesystem::absolute(entry.path(), entry_error);
        if (entry_error)
        {
            import_entry.full_path = entry.path();
        }
        m_snapshot_file_entries.push_back(std::move(import_entry));
    }

    std::sort(m_snapshot_file_entries.begin(), m_snapshot_file_entries.end(),
        [](const StateSnapshotFileEntry& lhs, const StateSnapshotFileEntry& rhs)
    {
        return lhs.file_name > rhs.file_name;
    });

    if (!m_snapshot_file_entries.empty())
    {
        m_snapshot_selected_index = 0;
        m_snapshot_io_status =
            "Found " + std::to_string(m_snapshot_file_entries.size()) + " local snapshot JSON file(s).";
    }
    else
    {
        m_snapshot_io_status = "No local snapshot JSON files found.";
    }
}

bool DemoBase::DeleteSelectedStateSnapshotJson()
{
    if (m_snapshot_selected_index < 0 ||
        m_snapshot_selected_index >= static_cast<int>(m_snapshot_file_entries.size()))
    {
        m_snapshot_io_status = "Snapshot delete failed: no local snapshot JSON is selected.";
        return false;
    }

    const StateSnapshotFileEntry selected_entry =
        m_snapshot_file_entries[static_cast<size_t>(m_snapshot_selected_index)];
    std::error_code fs_error;
    const bool exists = std::filesystem::exists(selected_entry.full_path, fs_error);
    if (fs_error)
    {
        m_snapshot_io_status =
            "Snapshot delete failed to query file state: " + selected_entry.full_path.string() +
            " (" + fs_error.message() + ")";
        return false;
    }

    if (!exists)
    {
        RefreshImportableStateSnapshotList();
        m_snapshot_io_status =
            "Snapshot delete skipped: file no longer exists: " + selected_entry.full_path.string();
        return false;
    }

    const bool removed = std::filesystem::remove(selected_entry.full_path, fs_error);
    if (!removed || fs_error)
    {
        m_snapshot_io_status =
            "Snapshot delete failed: " + selected_entry.full_path.string() +
            (fs_error ? (" (" + fs_error.message() + ")") : std::string{});
        return false;
    }

    const std::string removed_path = selected_entry.full_path.string();
    if (!m_last_snapshot_import_path.empty() &&
        m_last_snapshot_import_path == removed_path)
    {
        m_last_snapshot_import_path.clear();
    }

    RefreshImportableStateSnapshotList();
    m_snapshot_io_status = "Deleted snapshot JSON: " + removed_path;
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

bool DemoBase::RequestDemoSwitch(const std::string& demo_name)
{
    if (demo_name.empty() || demo_name == GetDemoCommandName() || !m_window)
    {
        return false;
    }

    m_pending_demo_switch_name = demo_name;
    m_window->RequestClose();
    return true;
}

void DemoBase::TickPendingRHISwitch(unsigned long long time_interval)
{
    (void)time_interval;
    if (!m_rhi_switch_requested)
    {
        m_rhi_switch_callback_installed = false;
        StartRenderGraphExecution();
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
        StartRenderGraphExecution();
        return true;
    }

    m_rhi_switch_in_progress = true;
    m_rhi_switch_last_error.clear();
    const auto non_render_state_snapshot = CaptureNonRenderStateSnapshot();

    const bool previous_per_frame_resource_binding = m_resource_manager->IsPerFrameResourceBindingEnabled();
    const auto previous_swapchain_policy = m_resource_manager->GetSwapchainResizePolicy();
    const auto previous_swapchain_present_mode = m_resource_manager->GetSwapchainPresentMode();
    const auto previous_validation_policy = m_render_graph->GetValidationPolicy();

    {
        std::string preload_error{};
        if (!PreloadRenderDocForDevice(m_pending_render_device_type, false, preload_error))
        {
            m_rhi_switch_last_error = preload_error;
            m_rhi_switch_requested = false;
            m_rhi_switch_in_progress = false;
            m_rhi_switch_callback_installed = false;
            return false;
        }
    }

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
    device.back_buffer_count = GetDefaultBackBufferCount(device.type, previous_swapchain_present_mode);
    device.swapchain_resize_policy = previous_swapchain_policy;
    device.swapchain_present_mode = previous_swapchain_present_mode;
    device.vulkan_optional_capabilities = GetRequestedVulkanOptionalCapabilities();

    if (!CreateRenderRuntimeContext(device, !m_debug_ui_enabled))
    {
        m_rhi_switch_last_error = "Failed to recreate runtime render context.";
        m_rhi_switch_requested = false;
        m_rhi_switch_in_progress = false;
        m_rhi_switch_callback_installed = false;
        return false;
    }
    m_resource_manager->SetPerFrameResourceBindingEnabled(previous_per_frame_resource_binding);
    m_swapchain_resize_policy_ui = previous_swapchain_policy;
    m_swapchain_resize_policy_ui_initialized = true;
    m_swapchain_present_mode_ui = previous_swapchain_present_mode;
    m_swapchain_present_mode_ui_initialized = true;
    m_render_graph->SetValidationPolicy(previous_validation_policy);

    if (!ReinitializeAfterRHIRecreate())
    {
        m_rhi_switch_last_error = "Failed to rebuild demo resources after RHI recreation.";
        m_rhi_switch_requested = false;
        m_rhi_switch_in_progress = false;
        m_rhi_switch_callback_installed = false;
        StartRenderGraphExecution();
        return false;
    }

    if (!ApplyNonRenderStateSnapshot(non_render_state_snapshot))
    {
        m_rhi_switch_last_error = "RHI switched, but failed to restore non-render state snapshot.";
    }

    StartRenderGraphExecution();
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
    return InitializeRuntimeModulesAndSystems();
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
