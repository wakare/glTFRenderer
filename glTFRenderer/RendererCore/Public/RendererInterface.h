#pragma once

#include <deque>
#include <functional>
#include <set>
#include <string>
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
    class RenderWindow
    {
    public:
        typedef std::function<void(unsigned long long)> RenderWindowTickCallback;
        
        RenderWindow(const RenderWindowDesc& desc);
        RenderWindowHandle GetHandle() const;
        unsigned GetWidth() const;
        unsigned GetHeight() const;
        HWND GetHWND() const;
        void EnterWindowEventLoop();

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
        
        ShaderHandle        CreateShader(const ShaderDesc& desc);
        TextureHandle       CreateTexture(const TextureDesc& desc);
        TextureHandle       CreateTexture(const TextureFileDesc& desc);
        BufferHandle        CreateBuffer(const BufferDesc& desc);
        IndexedBufferHandle CreateIndexedBuffer(const BufferDesc& desc);
        
        RenderTargetHandle  CreateRenderTarget(const RenderTargetDesc& desc);
        RenderTargetHandle  CreateRenderTarget(
            const std::string& name,
            unsigned width,
            unsigned height,
            PixelFormat format,
            RenderTargetClearValue clear_value,
            ResourceUsage usage);
        
        RenderPassHandle    CreateRenderPass(const RenderPassDesc& desc);
        RenderSceneHandle   CreateRenderScene(const RenderSceneDesc& desc);
        
        IRHIDevice&         GetDevice() const;
        IRHICommandQueue&   GetCommandQueue() const;
        IRHISwapChain&      GetCurrentSwapchain() const;
        unsigned            GetCurrentRenderWidth() const;
        unsigned            GetCurrentRenderHeight() const;
        IRHICommandList&    GetCommandListForRecordPassCommand(RenderPassHandle pass = NULL_HANDLE) const;
        IRHIDescriptorManager& GetDescriptorManager() const;
        IRHIMemoryManager&  GetMemoryManager() const;
        
        IRHITextureDescriptorAllocation& GetCurrentSwapchainRT() const;
        bool HasCurrentSwapchainRT() const;

        void UploadBufferData(BufferHandle handle, const BufferUploadDesc& upload_desc);
        void WaitFrameRenderFinished();
        void InvalidateSwapchainResizeRequest();
        bool ResizeSwapchainIfNeeded(unsigned width, unsigned height);
        bool ResizeWindowDependentRenderTargets(unsigned width, unsigned height);
        bool CleanupAllResources(bool clear_window_handles = false);
        
    protected:
        std::shared_ptr<ResourceManager> m_resource_manager;
        std::map<RenderPassHandle, std::shared_ptr<RenderPass>> m_render_passes;
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
            float cpu_time_ms{0.0f};
            bool gpu_time_valid{false};
            float gpu_time_ms{0.0f};
        };

        struct FrameStats
        {
            unsigned long long frame_index{0};
            float cpu_total_ms{0.0f};
            bool gpu_time_valid{false};
            float gpu_total_ms{0.0f};
            std::vector<RenderPassFrameStats> pass_stats;
        };
        
        typedef std::function<void(unsigned long long)> RenderGraphTickCallback;
        typedef std::function<void()> RenderGraphDebugUICallback;
        
        RenderGraph(ResourceOperator& allocator, RenderWindow& window);
        ~RenderGraph();
        
        RenderGraphNodeHandle CreateRenderGraphNode(const RenderGraphNodeDesc& render_graph_node_desc);
        RenderGraphNodeHandle CreateRenderGraphNode(ResourceOperator& allocator, const RenderPassSetupInfo& setup_info);
        
        bool RegisterRenderGraphNode(RenderGraphNodeHandle render_graph_node_handle);
        bool RemoveRenderGraphNode(RenderGraphNodeHandle render_graph_node_handle);
        bool UpdateComputeDispatch(RenderGraphNodeHandle render_graph_node_handle, unsigned group_size_x, unsigned group_size_y, unsigned group_size_z);
        
        bool CompileRenderPassAndExecute();

        void RegisterTextureToColorOutput(TextureHandle texture_handle);
        void RegisterRenderTargetToColorOutput(RenderTargetHandle render_target_handle);
        void RegisterTickCallback(const RenderGraphTickCallback& callback);
        void RegisterDebugUICallback(const RenderGraphDebugUICallback& callback);
        void EnableDebugUI(bool enable);
        const FrameStats& GetLastFrameStats() const;

    protected:
        struct DeferredReleaseEntry
        {
            unsigned long long retire_frame{0};
            std::vector<std::shared_ptr<IRHIResource>> resources;
        };

        struct RenderPassDescriptorResource
        {
            std::map<std::string, std::shared_ptr<IRHIBufferDescriptorAllocation>> m_buffer_descriptors;
            std::map<std::string, std::shared_ptr<IRHITextureDescriptorAllocation>> m_texture_descriptors;
            std::map<std::string, std::shared_ptr<IRHIDescriptorTable>> m_texture_descriptor_tables;
            std::map<std::string, std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>>> m_texture_descriptor_table_source_data;
            std::map<std::string, BufferBindingDesc> m_cached_buffer_bindings;
            std::map<std::string, TextureBindingDesc> m_cached_texture_bindings;
            std::map<std::string, RenderTargetTextureBindingDesc> m_cached_render_target_texture_bindings;
        };

        void EnqueueResourceForDeferredRelease(const std::shared_ptr<IRHIResource>& resource);
        void EnqueueBufferDescriptorForDeferredRelease(RenderPassDescriptorResource& descriptor_resource, const std::string& binding_name);
        void EnqueueTextureDescriptorForDeferredRelease(RenderPassDescriptorResource& descriptor_resource, const std::string& binding_name);
        void ReleaseRenderPassDescriptorResource(RenderPassDescriptorResource& descriptor_resource);
        void PruneDescriptorResources(RenderPassDescriptorResource& descriptor_resource, const RenderPassDrawDesc& draw_info);
        void CollectUnusedRenderPassDescriptorResources();
        void FlushDeferredResourceReleases(bool force_release_all);
        bool InitDebugUI();
        bool RenderDebugUI(IRHICommandList& command_list);
        void ShutdownDebugUI();
        bool InitGPUProfiler();
        void ShutdownGPUProfiler();
        void ResolveGPUProfilerFrame(unsigned slot_index);
        bool BeginGPUProfilerFrame(IRHICommandList& command_list, unsigned slot_index);
        bool WriteGPUProfilerTimestamp(IRHICommandList& command_list, unsigned slot_index, unsigned query_index);
        bool FinalizeGPUProfilerFrame(IRHICommandList& command_list, unsigned slot_index, unsigned query_count, const FrameStats& frame_stats);
        
        void ExecuteRenderGraphNode(IRHICommandList& command_list, RenderGraphNodeHandle render_graph_node_handle, unsigned long long interval);
        void CloseCurrentCommandListAndExecute(IRHICommandList& command_list, const RHIExecuteCommandListContext& context, bool wait);
        void Present(IRHICommandList& command_list);
        
        ResourceOperator& m_resource_allocator;
        RenderWindow& m_window;
        
        std::vector<RenderGraphNodeDesc> m_render_graph_nodes;
        std::set<RenderGraphNodeHandle> m_render_graph_node_handles;

        std::map<RenderGraphNodeHandle, RenderPassDescriptorResource> m_render_pass_descriptor_resources;
        std::map<RenderGraphNodeHandle, unsigned long long> m_render_pass_descriptor_last_used_frame;
        std::deque<DeferredReleaseEntry> m_deferred_release_entries;
        std::vector<RenderGraphNodeHandle> m_cached_execution_order;
        std::size_t m_cached_execution_signature{0};
        unsigned long long m_frame_index{0};
        
        std::shared_ptr<IRHITexture> m_final_color_output;
        TextureHandle m_final_color_output_texture_handle{NULL_HANDLE};
        RenderTargetHandle m_final_color_output_render_target_handle{NULL_HANDLE};
        RenderGraphTickCallback m_tick_callback;
        RenderGraphDebugUICallback m_debug_ui_callback;
        bool m_debug_ui_enabled{true};
        bool m_debug_ui_initialized{false};
        struct GPUProfilerState;
        std::unique_ptr<GPUProfilerState> m_gpu_profiler_state;
        FrameStats m_last_frame_stats{};
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
