#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#include <glm/glm/glm.hpp>

#include "Renderer.h"
#include "RendererCommon.h"
#include "RendererSceneAABB.h"

class IRHITexture;
class IRHIDescriptorTable;
class MaterialBase;
class RendererInputDevice;
class IRHIBufferDescriptorAllocation;
class IRHIDescriptorManager;
class IRHIDevice;
class IRHISwapChain;
class IRHITextureDescriptorAllocation;
class IRHIMemoryManager;
class IRHIResource;
class IRHICommandList;
class IRHICommandQueue;
class RenderPass;
class ResourceManager;
struct RHIExecuteCommandListContext;

namespace RendererInterface
{
    class RenderGraph;

    class RenderWindow
    {
    public:
        typedef std::function<void(unsigned long long)> RenderWindowTickCallback;
        struct WindowLoopTiming
        {
            bool valid{false};
            unsigned long long frame_index{0};
            float loop_total_ms{0.0f};
            float loop_thread_cpu_ms{0.0f};
            float idle_wait_ms{0.0f};
            float tick_callback_ms{0.0f};
            float tick_callback_thread_cpu_ms{0.0f};
            float poll_events_ms{0.0f};
            float poll_events_thread_cpu_ms{0.0f};
            float non_tick_ms{0.0f};
        };
        
        RenderWindow(const RenderWindowDesc& desc);
        RenderWindowHandle GetHandle() const;
        unsigned GetWidth() const;
        unsigned GetHeight() const;
        HWND GetHWND() const;
        void EnterWindowEventLoop();
        WindowLoopTiming GetLastLoopTiming() const;
        int GetWindowRefreshRate() const;
        void RequestClose();
        bool IsCloseRequested() const;

        void RegisterTickCallback(const RenderWindowTickCallback& callback);
        const RendererInputDevice& GetInputDeviceConst() const;
        RendererInputDevice& GetInputDevice();
        
    protected:
        
        RenderWindowDesc m_desc;
        RenderWindowHandle m_handle;
        HWND m_hwnd;
        std::shared_ptr<RendererInputDevice> m_input_device;
    };

    struct BufferUploadDesc
    {
        const void* data;
        size_t size;
    };

    class ResourceOperator
    {
    public:
        ResourceOperator(RenderDeviceDesc device);
        unsigned            GetCurrentBackBufferIndex() const;
        unsigned            GetCurrentFrameSlotIndex() const;
        unsigned            GetFrameSlotCount() const;
        unsigned            GetSwapchainImageCount() const;
        unsigned            GetBackBufferCount() const;
        FrameContextSnapshot GetFrameContext() const;
        bool                IsPerFrameResourceBindingEnabled() const;
        void                SetPerFrameResourceBindingEnabled(bool enable);
        
        ShaderHandle        CreateShader(const ShaderDesc& desc);
        TextureHandle       CreateTexture(const TextureDesc& desc);
        TextureHandle       CreateTexture(const TextureFileDesc& desc);
        BufferHandle        CreateBuffer(const BufferDesc& desc);
        IndexedBufferHandle CreateIndexedBuffer(const BufferDesc& desc);
        std::vector<BufferHandle> CreateFrameBufferedBuffers(const BufferDesc& desc, const std::string& debug_name_prefix = "");
        BufferHandle        GetFrameBufferedBufferHandle(const std::vector<BufferHandle>& buffers) const;
        BufferHandle        GetFrameBufferedBufferHandle(const std::vector<BufferHandle>& buffers, const FrameContextSnapshot& frame_context) const;
        void                UploadFrameBufferedBufferData(const std::vector<BufferHandle>& buffers, const BufferUploadDesc& upload_desc);
        
        RenderTargetHandle  CreateRenderTarget(const RenderTargetDesc& desc);
        std::vector<RenderTargetHandle> CreateFrameBufferedRenderTargets(const RenderTargetDesc& desc, const std::string& debug_name_prefix = "");
        RenderTargetHandle  GetFrameBufferedRenderTargetHandle(const std::vector<RenderTargetHandle>& render_targets) const;
        RenderTargetHandle  GetFrameBufferedRenderTargetHandle(const std::vector<RenderTargetHandle>& render_targets, const FrameContextSnapshot& frame_context) const;
        RenderTargetHandle  CreateFrameBufferedRenderTargetAlias(const RenderTargetDesc& desc, const std::string& debug_name_prefix = "");
        RenderTargetHandle  CreateRenderTarget(
            const std::string& name,
            unsigned width,
            unsigned height,
            PixelFormat format,
            RenderTargetClearValue clear_value,
            ResourceUsage usage);
        RenderTargetHandle  CreateFrameBufferedRenderTargetAlias(
            const std::string& name,
            unsigned width,
            unsigned height,
            PixelFormat format,
            RenderTargetClearValue clear_value,
            ResourceUsage usage);
        RenderTargetHandle  CreateWindowRelativeRenderTarget(
            const std::string& name,
            PixelFormat format,
            RenderTargetClearValue clear_value,
            ResourceUsage usage,
            float width_scale = 1.0f,
            float height_scale = 1.0f,
            unsigned min_width = 1,
            unsigned min_height = 1);
        RenderTargetHandle  CreateFrameBufferedWindowRelativeRenderTarget(
            const std::string& name,
            PixelFormat format,
            RenderTargetClearValue clear_value,
            ResourceUsage usage,
            float width_scale = 1.0f,
            float height_scale = 1.0f,
            unsigned min_width = 1,
            unsigned min_height = 1);
        
        RenderPassHandle    CreateRenderPass(const RenderPassDesc& desc);
        RenderSceneHandle   CreateRenderScene(const RenderSceneDesc& desc);
        
        IRHIDevice&         GetDevice() const;
        IRHICommandQueue&   GetCommandQueue() const;
        IRHISwapChain&      GetCurrentSwapchain() const;
        unsigned            GetCurrentRenderWidth() const;
        unsigned            GetCurrentRenderHeight() const;
        IRHICommandList&    GetCommandListForRecordPassCommand(RenderPassHandle pass = NULL_HANDLE) const;
        IRHICommandList&    GetCommandListForRecordPassCommand(const FrameContextSnapshot& frame_context, RenderPassHandle pass = NULL_HANDLE) const;
        IRHIDescriptorManager& GetDescriptorManager() const;
        IRHIMemoryManager&  GetMemoryManager() const;
        
        IRHITextureDescriptorAllocation& GetCurrentSwapchainRT() const;
        IRHITextureDescriptorAllocation& GetCurrentSwapchainRT(const FrameContextSnapshot& frame_context) const;
        bool HasCurrentSwapchainRT() const;

        void UploadBufferData(BufferHandle handle, const BufferUploadDesc& upload_desc);
        void BeginFrame();
        void AdvanceFrameSlot();
        void WaitFrameRenderFinished();
        void InvalidateSwapchainResizeRequest();
        WindowSurfaceSyncResult SyncWindowSurface(unsigned window_width, unsigned window_height);
        void NotifySwapchainAcquireFailure();
        void NotifySwapchainPresentFailure();
        bool ResizeSwapchainIfNeeded(unsigned width, unsigned height);
        bool ResizeWindowDependentRenderTargets(unsigned width, unsigned height);
        SwapchainLifecycleState GetSwapchainLifecycleState() const;
        SwapchainResizePolicy GetSwapchainResizePolicy() const;
        void SetSwapchainResizePolicy(const SwapchainResizePolicy& policy, bool reset_retry_state = true);
        SwapchainPresentMode GetSwapchainPresentMode() const;
        void SetSwapchainPresentMode(SwapchainPresentMode mode);
        void ApplyFrameBufferedRenderTargetAliases();
        bool CleanupAllResources(bool clear_window_handles = false);
        
    protected:
        std::shared_ptr<ResourceManager> m_resource_manager;
        std::map<RenderPassHandle, std::shared_ptr<RenderPass>> m_render_passes;
        bool m_per_frame_resource_binding_enabled{true};
        std::map<RenderTargetHandle, std::vector<RenderTargetHandle>> m_frame_buffered_render_target_aliases;
        std::map<RenderTargetHandle, RenderTargetHandle> m_frame_buffered_render_target_alias_current;
    };

    class RendererModuleBase
    {
    public:
        IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(RendererModuleBase)
        virtual bool FinalizeModule(RendererInterface::ResourceOperator& resource_operator) = 0;
        virtual bool BindDrawCommands(RendererInterface::RenderPassDrawDesc& out_draw_desc) = 0;
        virtual bool Tick(RendererInterface::ResourceOperator&, unsigned long long interval) {return true; };
    };
    
    class RenderGraph
    {
    public:

        struct RenderPassSetupInfo
        {
            struct ShaderSetupInfo
            {
                ShaderType shader_type;
                std::string entry_function;
                std::string shader_file;
            };

            struct RenderTargetTextureArrayBindingDesc
            {
                std::vector<RenderTargetHandle> render_targets;
                RenderTargetTextureBindingDesc m_binding_desc;
            };
            
            RenderPassType render_pass_type;
            
            std::vector<ShaderSetupInfo> shader_setup_infos;
            std::map<RenderTargetHandle, RenderTargetBindingDesc> render_targets;
            std::vector<RenderTargetTextureBindingDesc> sampled_render_targets;
            std::map<std::string, BufferBindingDesc> buffer_resources;
            std::map<std::string, TextureBindingDesc> texture_resources;
            std::set<std::string> excluded_buffer_bindings;
            std::set<std::string> excluded_texture_bindings;
            std::set<std::string> excluded_render_target_texture_bindings;
            std::vector<std::shared_ptr<RendererModuleBase>> modules;

            std::vector<RenderExecuteCommand> execute_commands;
            std::optional<RenderExecuteCommand> execute_command;

            RenderStateDesc render_state{};
            std::vector<RenderGraphNodeHandle> dependency_render_graph_nodes;
            std::function<void(unsigned long long)> pre_render_callback;
            std::string debug_group;
            std::string debug_name;

            int viewport_width{-1};
            int viewport_height{-1};
        };

        struct RenderPassFrameStats
        {
            RenderGraphNodeHandle node_handle{};
            std::string group_name;
            std::string pass_name;
            RenderPassType pass_type{RenderPassType::GRAPHICS};
            bool executed{true};
            bool skipped_due_to_validation{false};
            float cpu_time_ms{0.0f};
            bool gpu_time_valid{false};
            float gpu_time_ms{0.0f};
        };

        struct FrameStats
        {
            unsigned long long frame_index{0};
            float cpu_total_ms{0.0f};
            float cpu_executed_ms{0.0f};
            float cpu_skipped_ms{0.0f};
            float cpu_skipped_validation_ms{0.0f};
            bool gpu_time_valid{false};
            float gpu_total_ms{0.0f};
            unsigned total_pass_count{0};
            unsigned executed_pass_count{0};
            unsigned skipped_pass_count{0};
            unsigned skipped_validation_pass_count{0};
            unsigned skipped_missing_pass_count{0};
            unsigned graphics_pass_count{0};
            unsigned compute_pass_count{0};
            unsigned ray_tracing_pass_count{0};
            unsigned executed_graphics_pass_count{0};
            unsigned executed_compute_pass_count{0};
            unsigned executed_ray_tracing_pass_count{0};
            std::vector<RenderPassFrameStats> pass_stats;
        };

        struct FrameTimingBreakdown
        {
            unsigned long long frame_index{0};
            bool valid{false};
            float frame_total_ms{0.0f};
            float prepare_frame_ms{0.0f};
            float sync_window_surface_ms{0.0f};
            float tick_and_debug_ui_build_ms{0.0f};
            float tick_callback_ms{0.0f};
            float tick_other_ms{0.0f};
            float module_tick_ms{0.0f};
            float system_tick_ms{0.0f};
            float debug_ui_build_ms{0.0f};
            float wait_previous_frame_ms{0.0f};
            float deferred_release_ms{0.0f};
            float acquire_context_ms{0.0f};
            float resolve_gpu_profiler_ms{0.0f};
            float acquire_command_list_ms{0.0f};
            float acquire_swapchain_ms{0.0f};
            float execute_render_graph_ms{0.0f};
            float execution_planning_ms{0.0f};
            float execute_passes_ms{0.0f};
            float collect_unused_descriptor_ms{0.0f};
            float blit_to_swapchain_ms{0.0f};
            float finalize_submission_ms{0.0f};
            float render_debug_ui_ms{0.0f};
            float present_ms{0.0f};
            float submit_command_list_ms{0.0f};
            float present_call_ms{0.0f};
            float frame_wait_total_ms{0.0f};
            float non_pass_cpu_ms{0.0f};
            float untracked_ms{0.0f};
        };

        struct DependencyDiagnostics
        {
            struct AutoPrunedNodeDiagnostics
            {
                RenderGraphNodeHandle node_handle{};
                std::string group_name;
                std::string pass_name;
                unsigned buffer_count{0};
                unsigned texture_count{0};
                unsigned render_target_texture_count{0};
            };

            struct CrossFrameResourceHazardDiagnostics
            {
                enum class HazardType
                {
                    PREVIOUS_WRITE_CURRENT_READ,
                    PREVIOUS_WRITE_CURRENT_WRITE,
                    PREVIOUS_READ_CURRENT_WRITE,
                };

                HazardType hazard_type{HazardType::PREVIOUS_WRITE_CURRENT_READ};
                std::string resource_kind;
                unsigned resource_id{0};
                std::vector<std::string> previous_passes;
                std::vector<std::string> current_passes;
            };

            bool graph_valid{true};
            bool has_invalid_explicit_dependencies{false};
            unsigned auto_merged_dependency_count{0};
            unsigned auto_pruned_binding_count{0};
            unsigned auto_pruned_node_count{0};
            bool cross_frame_analysis_ready{false};
            unsigned cross_frame_comparison_window_size{0};
            unsigned cross_frame_compared_frame_count{0};
            unsigned cross_frame_hazard_count{0};
            unsigned cross_frame_hazard_overflow_count{0};
            std::vector<std::pair<RenderGraphNodeHandle, RenderGraphNodeHandle>> invalid_explicit_dependencies;
            std::vector<RenderGraphNodeHandle> cycle_nodes;
            std::vector<AutoPrunedNodeDiagnostics> auto_pruned_nodes;
            std::vector<CrossFrameResourceHazardDiagnostics> cross_frame_hazards;
            std::size_t execution_signature{0};
            std::size_t cached_execution_node_count{0};
            std::size_t cached_execution_order_size{0};
        };

        struct ValidationPolicy
        {
            unsigned log_interval_frames{120};
            unsigned cross_frame_hazard_check_interval_frames{8};
            bool skip_execution_on_warning{false};
        };
        
        typedef std::function<void(unsigned long long)> RenderGraphTickCallback;
        typedef std::function<void()> RenderGraphDebugUICallback;

        struct ExecutionPlanContext
        {
            const std::vector<RenderGraphNodeHandle>& nodes;
            const std::vector<RenderGraphNodeDesc>& render_graph_nodes;
            const std::set<RenderGraphNodeHandle>& registered_nodes;
            const std::vector<RenderGraphNodeHandle>& cached_execution_order;
            bool cached_execution_graph_valid{true};
            std::size_t cached_execution_signature{0};
            std::size_t cached_execution_node_count{0};
        };

        struct ExecutionPlanCacheState
        {
            bool& cached_execution_graph_valid;
            std::vector<RenderGraphNodeHandle>& cached_execution_order;
            std::size_t& cached_execution_signature;
            std::size_t& cached_execution_node_count;
        };
        
        RenderGraph(ResourceOperator& allocator, RenderWindow& window);
        ~RenderGraph();
        
        RenderGraphNodeHandle CreateRenderGraphNode(const RenderGraphNodeDesc& render_graph_node_desc);
        RenderGraphNodeHandle CreateRenderGraphNode(ResourceOperator& allocator, const RenderPassSetupInfo& setup_info);
        
        bool RegisterRenderGraphNode(RenderGraphNodeHandle render_graph_node_handle);
        bool RemoveRenderGraphNode(RenderGraphNodeHandle render_graph_node_handle);
        bool UpdateComputeDispatch(RenderGraphNodeHandle render_graph_node_handle, unsigned group_size_x, unsigned group_size_y, unsigned group_size_z);
        bool QueueNodeRenderStateUpdate(RenderGraphNodeHandle render_graph_node_handle, const RenderStateDesc& render_state);
        bool UpdateNodeBufferBinding(RenderGraphNodeHandle render_graph_node_handle, const std::string& binding_name, BufferHandle buffer_handle);
        bool UpdateNodeRenderTargetBinding(RenderGraphNodeHandle render_graph_node_handle, RenderTargetHandle old_render_target_handle, RenderTargetHandle new_render_target_handle);
        bool UpdateNodeRenderTargetTextureBinding(RenderGraphNodeHandle render_graph_node_handle, const std::string& binding_name, const std::vector<RenderTargetHandle>& render_target_handles);
        bool UpdateNodeRenderTargetTextureBinding(RenderGraphNodeHandle render_graph_node_handle, const std::string& binding_name, RenderTargetHandle render_target_handle);
        
        bool CompileRenderPassAndExecute();

        void RegisterTextureToColorOutput(TextureHandle texture_handle);
        void RegisterRenderTargetToColorOutput(RenderTargetHandle render_target_handle);
        void RegisterTickCallback(const RenderGraphTickCallback& callback);
        void RegisterDebugUICallback(const RenderGraphDebugUICallback& callback);
        void EnableDebugUI(bool enable);
        void ShutdownRuntimeServices();
        void SetValidationPolicy(const ValidationPolicy& policy);
        ValidationPolicy GetValidationPolicy() const;
        const FrameStats& GetLastFrameStats() const;
        const FrameTimingBreakdown& GetLastFrameTimingBreakdown() const;
        const DependencyDiagnostics& GetDependencyDiagnostics() const;
        void SetTickCallbackBreakdown(float other_ms, float module_ms, float system_ms);

    protected:
        enum class RenderPassExecutionStatus
        {
            EXECUTED,
            SKIPPED_INVALID_DRAW_DESC,
            SKIPPED_MISSING_RENDER_PASS,
        };

        struct DeferredReleaseEntry
        {
            unsigned long long retire_frame{0};
            std::vector<std::shared_ptr<IRHIResource>> resources;
        };

        struct DeferredReleaseQueue
        {
            std::deque<DeferredReleaseEntry> entries;

            void Enqueue(const std::shared_ptr<IRHIResource>& resource, unsigned long long current_frame, unsigned delay_frame);
            void Flush(IRHIMemoryManager& memory_manager, unsigned long long current_frame, bool force_release_all);
            void Clear();
        };

        struct PendingRenderStateUpdate
        {
            RenderStateDesc render_state{};
        };

        struct RenderPassDescriptorResource
        {
            struct BufferDescriptorCacheEntry
            {
                BufferBindingDesc binding_desc{};
                std::uintptr_t buffer_identity_key{0};
                std::shared_ptr<IRHIBufferDescriptorAllocation> descriptor{};
                unsigned long long last_used_frame{0};
            };

            struct TextureDescriptorCacheEntry
            {
                unsigned binding_type{0};
                std::vector<std::uintptr_t> source_texture_identity_keys;
                std::shared_ptr<IRHITextureDescriptorAllocation> descriptor{};
                std::shared_ptr<IRHIDescriptorTable> descriptor_table{};
                std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> descriptor_table_source_data;
                unsigned long long last_used_frame{0};
            };

            // key: binding_name -> cache_key -> descriptor entry
            std::map<std::string, std::map<unsigned long long, BufferDescriptorCacheEntry>> m_buffer_descriptor_cache;
            // key: binding_name -> cache_key -> descriptor entry (TextureHandle path)
            std::map<std::string, std::map<unsigned long long, TextureDescriptorCacheEntry>> m_texture_descriptor_cache;
            // key: binding_name -> cache_key -> descriptor entry (RenderTargetHandle path)
            std::map<std::string, std::map<unsigned long long, TextureDescriptorCacheEntry>> m_render_target_texture_descriptor_cache;
        };

        struct DescriptorResourceStore
        {
            std::map<RenderGraphNodeHandle, RenderPassDescriptorResource> resources;
            std::map<RenderGraphNodeHandle, unsigned long long> last_used_frames;

            RenderPassDescriptorResource* Find(RenderGraphNodeHandle node_handle);
            const RenderPassDescriptorResource* Find(RenderGraphNodeHandle node_handle) const;
            RenderPassDescriptorResource& GetOrCreate(RenderGraphNodeHandle node_handle);
            void MarkUsed(RenderGraphNodeHandle node_handle, unsigned long long frame_index);
            void Erase(RenderGraphNodeHandle node_handle);
            std::vector<RenderGraphNodeHandle> CollectExpiredInactiveHandles(
                const std::set<RenderGraphNodeHandle>& active_node_handles,
                unsigned long long current_frame,
                unsigned retention_frame) const;
        };

        struct ExecutionPlanState
        {
            std::vector<RenderGraphNodeHandle> cached_execution_order;
            std::size_t cached_execution_signature{0};
            std::size_t cached_execution_node_count{0};
            std::size_t last_active_node_set_signature{0};
            std::size_t last_active_node_set_count{0};
            bool cached_execution_graph_valid{true};
            bool plan_dirty{true};

            void MarkDirty();
            void ResetCache();
            bool UpdateActiveNodeSet(std::size_t active_node_set_signature, std::size_t active_node_set_count);
            bool CollectActiveNodes(const std::set<RenderGraphNodeHandle>& active_node_handles, std::vector<RenderGraphNodeHandle>& out_nodes);
            ExecutionPlanContext BuildContext(
                const std::vector<RenderGraphNodeHandle>& nodes,
                const std::vector<RenderGraphNodeDesc>& render_graph_nodes,
                const std::set<RenderGraphNodeHandle>& registered_nodes) const;
            ExecutionPlanCacheState BuildCacheState();
            bool IsCachedExecutionOrderMissing() const;
            bool ShouldRebuild(bool active_node_set_changed, bool should_update_dependency_diagnostics) const;
            void MarkPlanApplied();
        };

        struct FrameResourceAccessSnapshot
        {
            std::map<unsigned long long, unsigned char> access_masks;
            // value.first = readers, value.second = writers
            std::map<unsigned long long, std::pair<std::vector<std::string>, std::vector<std::string>>> pass_accesses;
            unsigned long long frame_index{0};
        };

        struct DependencyDiagnosticsState
        {
            unsigned long long last_update_frame_index{0};
            DependencyDiagnostics diagnostics{};
            std::vector<FrameResourceAccessSnapshot> frame_slot_resource_access_snapshots;
            std::vector<unsigned char> frame_slot_resource_access_snapshot_valid;

            void Reset();
            bool ShouldUpdate(unsigned long long current_frame, unsigned interval_frames) const;
            void MarkUpdated(unsigned long long current_frame);
            void EnsureCrossFrameHazardSnapshotStorage(unsigned window_size);
            const FrameResourceAccessSnapshot* GetCrossFrameHazardSnapshot(unsigned hazard_slot_index) const;
            void UpdateCrossFrameHazardSnapshot(unsigned hazard_slot_index, FrameResourceAccessSnapshot snapshot);
        };

        struct FramePreparationContext
        {
            unsigned window_width{0};
            unsigned window_height{0};
            FrameContextSnapshot resource_frame_context{};
            FrameContextSnapshot presentation_frame_context{};
            unsigned profiler_slot_index{0};
            bool require_explicit_frame_wait{false};
            IRHICommandList* command_list{nullptr};
        };

        void EnqueueResourceForDeferredRelease(const std::shared_ptr<IRHIResource>& resource, const FrameContextSnapshot& frame_context);
        void EnqueueBufferDescriptorEntryForDeferredRelease(const RenderPassDescriptorResource::BufferDescriptorCacheEntry& cache_entry, const FrameContextSnapshot& frame_context);
        void EnqueueTextureDescriptorEntryForDeferredRelease(const RenderPassDescriptorResource::TextureDescriptorCacheEntry& cache_entry, const FrameContextSnapshot& frame_context);
        void EnqueueBufferDescriptorForDeferredRelease(RenderPassDescriptorResource& descriptor_resource, const std::string& binding_name, const FrameContextSnapshot& frame_context);
        void EnqueueTextureDescriptorForDeferredRelease(RenderPassDescriptorResource& descriptor_resource, const std::string& binding_name, const FrameContextSnapshot& frame_context);
        void ReleaseRenderPassDescriptorResource(RenderPassDescriptorResource& descriptor_resource, const FrameContextSnapshot& frame_context);
        void PruneDescriptorResources(RenderPassDescriptorResource& descriptor_resource, const RenderPassDrawDesc& draw_info, const FrameContextSnapshot& frame_context);
        void CollectUnusedRenderPassDescriptorResources(const FrameContextSnapshot& frame_context);
        void FlushDeferredResourceReleases(bool force_release_all);
        void ApplyPendingRenderStateUpdates(const FrameContextSnapshot& frame_context);
        bool InitDebugUI();
        bool RenderDebugUI(IRHICommandList& command_list, const FrameContextSnapshot& frame_context);
        void ShutdownDebugUI();
        bool InitGPUProfiler();
        void ShutdownGPUProfiler();
        bool HasValidGPUProfilerSlot(unsigned slot_index) const;
        unsigned GetGPUProfilerMaxTimestampedPassCount() const;
        unsigned GetGPUProfilerMaxQueryCount() const;
        unsigned ClampGPUProfilerQueryCount(unsigned query_count) const;
        void ResolveGPUProfilerFrame(unsigned slot_index);
        bool BeginGPUProfilerFrame(IRHICommandList& command_list, unsigned slot_index);
        bool WriteGPUProfilerTimestamp(IRHICommandList& command_list, unsigned slot_index, unsigned query_index);
        bool FinalizeGPUProfilerFrame(IRHICommandList& command_list, unsigned slot_index, unsigned query_count, const FrameStats& frame_stats);
        bool ResolveFinalColorOutput();
        void ExecuteTickAndDebugUI(unsigned long long interval);
        bool SyncWindowSurfaceAndAdvanceFrame(FramePreparationContext& frame_context, unsigned long long interval);
        bool AcquireCurrentFrameCommandContext(FramePreparationContext& frame_context);
        void OnFrameTick(unsigned long long interval);
        bool PrepareFrameForRendering(unsigned long long interval, FramePreparationContext& frame_context);
        void ExecutePlanAndCollectStats(IRHICommandList& command_list, const FrameContextSnapshot& frame_context, unsigned profiler_slot_index, unsigned long long interval);
        void ExecuteRenderGraphFrame(FramePreparationContext& frame_context, unsigned long long interval);
        bool AcquireCurrentFrameSwapchain(FramePreparationContext& frame_context);
        void BlitFinalOutputToSwapchain(IRHICommandList& command_list, const FrameContextSnapshot& frame_context, unsigned window_width, unsigned window_height);
        void FinalizeFrameSubmission(FramePreparationContext& frame_context, bool swapchain_ready);
        
        RenderPassExecutionStatus ExecuteRenderGraphNode(IRHICommandList& command_list, const FrameContextSnapshot& frame_context, RenderGraphNodeHandle render_graph_node_handle, unsigned long long interval);
        void LogRenderPassValidationResult(RenderGraphNodeHandle render_graph_node_handle,
                                           const RenderGraphNodeDesc& render_graph_node_desc,
                                           bool valid,
                                           const std::vector<std::string>& errors,
                                           const std::vector<std::string>& warnings);
        void CloseCurrentCommandListAndExecute(IRHICommandList& command_list, const RHIExecuteCommandListContext& context, bool wait);
        void Present(IRHICommandList& command_list, const FrameContextSnapshot& frame_context);
        
        ResourceOperator& m_resource_allocator;
        RenderWindow& m_window;
        
        std::vector<RenderGraphNodeDesc> m_render_graph_nodes;
        std::set<RenderGraphNodeHandle> m_render_graph_node_handles;

        DescriptorResourceStore m_descriptor_resource_store;
        ExecutionPlanState m_execution_plan_state;
        DependencyDiagnosticsState m_dependency_diagnostics_state;
        std::map<RenderGraphNodeHandle, std::tuple<unsigned, unsigned, unsigned>> m_auto_pruned_named_binding_counts;
        std::map<RenderGraphNodeHandle, unsigned long long> m_render_pass_validation_last_log_frame;
        std::map<RenderGraphNodeHandle, std::size_t> m_render_pass_validation_last_message_hash;
        std::map<RenderGraphNodeHandle, PendingRenderStateUpdate> m_pending_render_state_updates;
        DeferredReleaseQueue m_deferred_release_queue;
        unsigned GetCrossFrameComparisonWindowSize(const FrameContextSnapshot& frame_context) const;
        unsigned GetCrossFrameHazardSlotIndex(const FrameContextSnapshot& frame_context, unsigned window_size) const;
        unsigned long long m_frame_index{0};
        
        std::shared_ptr<IRHITexture> m_final_color_output;
        TextureHandle m_final_color_output_texture_handle{NULL_HANDLE};
        RenderTargetHandle m_final_color_output_render_target_handle{NULL_HANDLE};
        RenderGraphTickCallback m_tick_callback;
        RenderGraphDebugUICallback m_debug_ui_callback;
        bool m_debug_ui_enabled{true};
        bool m_debug_ui_initialized{false};
        ValidationPolicy m_validation_policy{};
        struct GPUProfilerState;
        std::unique_ptr<GPUProfilerState> m_gpu_profiler_state;
        FrameStats m_last_frame_stats{};
        FrameTimingBreakdown m_current_frame_timing_breakdown{};
        FrameTimingBreakdown m_last_frame_timing_breakdown{};
    };

    class RendererSceneMeshDataAccessorBase
    {
    public:
        enum class MeshDataAccessorType
        {
            VERTEX_POSITION_FLOAT3,
            VERTEX_NORMAL_FLOAT3,
            VERTEX_TANGENT_FLOAT4,
            VERTEX_TEXCOORD0_FLOAT2,
            INDEX_INT,
            INDEX_HALF,
            INSTANCE_MAT4x4,
        };

        virtual bool HasMeshData(unsigned mesh_id) const = 0;
        virtual void AccessMeshData(MeshDataAccessorType type, unsigned mesh_id, void* data, size_t element_size) = 0;
        virtual void AccessInstanceData(MeshDataAccessorType type, unsigned instance_id, unsigned mesh_id, void* data, size_t element_size) = 0;

        virtual void AccessMaterialData(const MaterialBase& material, unsigned mesh_id) = 0;
    };

    // Framework-level teardown entry for runtime RHI recreation / shutdown.
    bool CleanupRenderRuntimeContext(
        std::shared_ptr<RenderGraph>& render_graph,
        std::shared_ptr<ResourceOperator>& resource_operator,
        bool clear_window_handles = false);
    
    class RendererSceneResourceManager
    {
    public:
        RendererSceneResourceManager(ResourceOperator& allocator,const RenderSceneDesc& desc);

        bool AccessSceneData(RendererSceneMeshDataAccessorBase& data_accessor);
        RendererSceneAABB GetSceneBounds() const;
        
    protected:
        ResourceOperator& m_allocator;
        RenderSceneHandle m_render_scene_handle {NULL_HANDLE};
    };
}
