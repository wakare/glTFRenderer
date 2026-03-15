#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "RendererCommon.h"
#include "RendererInterface.h"
#include <nlohmann_json/single_include/nlohmann/json.hpp>

class RendererSystemBase;

class DemoBase
{
public:
    struct NonRenderStateSnapshot
    {
        virtual ~NonRenderStateSnapshot() = default;
    };

    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DemoBase)
    bool Init(const std::vector<std::string>& arguments);
    
    virtual void Run();
    void Shutdown();
    bool HasPendingDemoSwitch() const { return !m_pending_demo_switch_name.empty(); }
    const std::string& GetPendingDemoSwitchName() const { return m_pending_demo_switch_name; }
    std::vector<std::string> BuildLaunchArgumentsForDemo(const std::string& demo_name) const;
    
    bool InitRenderContext(const std::vector<std::string>& arguments);
    RendererInterface::ShaderHandle CreateShader(RendererInterface::ShaderType type, const std::string& source, const std::string& entry_function);
    RendererInterface::RenderTargetHandle CreateRenderTarget(
        const std::string& name,
        unsigned width,
        unsigned height,
        RendererInterface::PixelFormat format,
        RendererInterface::RenderTargetClearValue clear_value,
        RendererInterface::ResourceUsage usage);
    
protected:
    struct StateSnapshotFileEntry
    {
        std::string file_name{};
        std::filesystem::path full_path{};
    };

    void TickFrame(unsigned long long time_interval);
    void DrawDebugUI();
    bool RecordCurrentStateSnapshot();
    bool RestoreRecordedStateSnapshot();
    void ClearRecordedStateSnapshot();
    bool ExportCurrentStateSnapshotToJson();
    bool ImportStateSnapshotFromJson(const std::filesystem::path& snapshot_path);
    void RefreshImportableStateSnapshotList();
    bool DeleteSelectedStateSnapshotJson();
    void PollPendingRenderDocCapture();
    bool PreloadRenderDocForDevice(RendererInterface::RenderDeviceType device_type,
                                   bool log_status,
                                   std::string& out_error);
    bool EnsureRenderDocCaptureConfigured(bool require_available, std::string& out_status);
    std::filesystem::path BuildManualRenderDocCapturePath() const;
    bool QueueManualRenderDocCapture(bool auto_open_replay_ui);
    bool OpenLastRenderDocCaptureInReplayUI();
    bool CreateRenderRuntimeContext(const RendererInterface::RenderDeviceDesc& device_desc, bool disable_debug_ui);
    bool InitializeRuntimeModulesAndSystems();
    void StartRenderGraphExecution();
    bool RequestRuntimeRHISwitch(RendererInterface::RenderDeviceType new_device_type);
    bool RequestDemoSwitch(const std::string& demo_name);
    void TickPendingRHISwitch(unsigned long long time_interval);
    bool ExecutePendingRHISwitch();
    virtual bool ReinitializeAfterRHIRecreate();
    virtual bool RebuildRenderRuntimeObjects();
    virtual std::shared_ptr<NonRenderStateSnapshot> CaptureNonRenderStateSnapshot() const { return nullptr; }
    virtual bool ApplyNonRenderStateSnapshot(const std::shared_ptr<NonRenderStateSnapshot>& snapshot)
    {
        (void)snapshot;
        return true;
    }
    virtual bool SerializeNonRenderStateSnapshotToJson(
        const std::shared_ptr<NonRenderStateSnapshot>& snapshot,
        nlohmann::json& out_snapshot_json) const
    {
        (void)snapshot;
        (void)out_snapshot_json;
        return false;
    }
    virtual std::shared_ptr<NonRenderStateSnapshot> DeserializeNonRenderStateSnapshotFromJson(
        const nlohmann::json& snapshot_json,
        std::string& out_error) const
    {
        (void)snapshot_json;
        out_error = "Snapshot import is not implemented for this demo.";
        return nullptr;
    }
    
    virtual bool InitInternal(const std::vector<std::string>& arguments) = 0;
    virtual void TickFrameInternal(unsigned long long time_interval);
    virtual void DrawDebugUIInternal() {}
    virtual const char* GetDemoPanelName() const { return "RendererDemo"; }
    virtual const char* GetDemoCommandName() const { return GetSnapshotTypeName(); }
    virtual const char* GetSnapshotTypeName() const { return GetDemoPanelName(); }
    virtual RendererInterface::VulkanOptionalCapabilities GetRequestedVulkanOptionalCapabilities() const { return {}; }
    void CleanupWindowBoundRuntimeIfNeeded(bool clear_window_handles);
    
    unsigned m_width{1920};
    unsigned m_height{1080};
        
    std::shared_ptr<RendererInterface::RenderWindow> m_window;
    std::shared_ptr<RendererInterface::ResourceOperator> m_resource_manager;
    std::shared_ptr<RendererInterface::RenderGraph> m_render_graph;

    std::vector<std::shared_ptr<RendererInterface::RendererModuleBase>> m_modules;
    std::vector<std::shared_ptr<RendererSystemBase>> m_systems;
    std::map<RendererInterface::RenderTargetHandle, RendererInterface::RenderTargetDesc> m_render_target_desc_infos;
    unsigned m_last_render_width{0};
    unsigned m_last_render_height{0};
    std::vector<std::string> m_launch_arguments;
    RendererInterface::RenderDeviceType m_render_device_type{RendererInterface::DX12};
    RendererInterface::RenderDeviceType m_pending_render_device_type{RendererInterface::DX12};
    RendererInterface::RenderDeviceType m_runtime_rhi_ui_selection{RendererInterface::DX12};
    bool m_rhi_switch_requested{false};
    bool m_rhi_switch_callback_installed{false};
    bool m_rhi_switch_in_progress{false};
    bool m_debug_ui_enabled{true};
    std::string m_rhi_switch_last_error{};
    std::string m_pending_demo_switch_name{};
    bool m_window_runtime_cleaned_up{false};
    std::shared_ptr<NonRenderStateSnapshot> m_recorded_state_snapshot{};
    std::string m_recorded_state_snapshot_status{};
    char m_snapshot_export_name[128]{"snapshot"};
    char m_snapshot_directory[260]{"build_logs/snapshots"};
    std::filesystem::path m_startup_snapshot_path{};
    std::vector<StateSnapshotFileEntry> m_snapshot_file_entries{};
    int m_snapshot_selected_index{-1};
    std::string m_last_snapshot_export_path{};
    std::string m_last_snapshot_import_path{};
    std::string m_snapshot_io_status{};
    char m_renderdoc_capture_name[128]{"capture"};
    char m_renderdoc_capture_directory[260]{"build_logs/renderdoc"};
    bool m_renderdoc_preload_requested{false};
    bool m_renderdoc_required{false};
    bool m_renderdoc_auto_open_after_capture{false};
    bool m_renderdoc_manual_capture_pending{false};
    bool m_renderdoc_manual_capture_auto_open{false};
    unsigned long long m_renderdoc_manual_capture_frame_index{0};
    std::filesystem::path m_renderdoc_manual_capture_requested_path{};
    std::string m_renderdoc_startup_status{};
    std::string m_renderdoc_ui_status{};
    RendererInterface::SwapchainResizePolicy m_swapchain_resize_policy_ui{};
    bool m_swapchain_resize_policy_ui_initialized{false};
    RendererInterface::SwapchainPresentMode m_swapchain_present_mode_ui{RendererInterface::SwapchainPresentMode::VSYNC};
    bool m_swapchain_present_mode_ui_initialized{false};
};
