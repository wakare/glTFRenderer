#pragma once
#include "Renderer.h"

#include <deque>

enum class RHIPipelineType;
enum class RHIDataFormat;
class IRHIRenderTarget;
class IRHIShader;
class IRHITextureAllocation;
class IRHITexture;
class glTFRenderResourceFrameManager;
class IRHIPipelineStateObject;
class glTFRenderMeshManager;
class glTFRenderMaterialManager;
class IRHITextureDescriptorAllocation;
class IRHIRenderTargetManager;
class IRHICommandList;
class IRHICommandAllocator;
class IRHIMemoryManager;
class IRHISwapChain;
class IRHICommandQueue;
class IRHIDevice;
class IRHIFactory;
class IRHIResource;

namespace RendererInterfaceRHIConverter
{
    RHIDataFormat ConvertToRHIFormat(RendererInterface::PixelFormat format);
    RHIPipelineType ConvertToRHIPipelineType(RendererInterface::RenderPassType type);
}

class ResourceManager
{
public:
    bool InitResourceManager(const RendererInterface::RenderDeviceDesc& desc);

    RendererInterface::BufferHandle CreateBuffer(const RendererInterface::BufferDesc& desc);
    RendererInterface::IndexedBufferHandle CreateIndexedBuffer(const RendererInterface::BufferDesc& desc);
    RendererInterface::ShaderHandle CreateShader(const RendererInterface::ShaderDesc& shader_desc);
    RendererInterface::RenderTargetHandle CreateRenderTarget(const RendererInterface::RenderTargetDesc& desc);

    unsigned GetCurrentBackBufferIndex() const;
    
    IRHIDevice& GetDevice();
    IRHISwapChain& GetSwapChain();
    IRHIMemoryManager& GetMemoryManager();

    void WaitFrameRenderFinished();
    void WaitGPUIdle();
    void InvalidateSwapchainResizeRequest();
    RendererInterface::WindowSurfaceSyncResult SyncWindowSurface(unsigned window_width, unsigned window_height);
    void NotifySwapchainAcquireFailure();
    void NotifySwapchainPresentFailure();
    bool ResizeSwapchainIfNeeded(unsigned width, unsigned height);
    bool ResizeWindowDependentRenderTargets(unsigned width, unsigned height);
    RendererInterface::SwapchainLifecycleState GetSwapchainLifecycleState() const;
    RendererInterface::SwapchainResizePolicy GetSwapchainResizePolicy() const;
    void SetSwapchainResizePolicy(const RendererInterface::SwapchainResizePolicy& policy, bool reset_retry_state = true);
    IRHICommandList& GetCommandListForRecordPassCommand(RendererInterface::RenderPassHandle render_pass_handle = NULL_HANDLE);

    IRHICommandQueue& GetCommandQueue();

    IRHITextureDescriptorAllocation& GetCurrentSwapchainRT();
    bool HasCurrentSwapchainRT() const;
    
protected:
    RendererInterface::RenderDeviceDesc m_device_desc{};
    
    std::shared_ptr<IRHIFactory> m_factory;
    std::shared_ptr<IRHIDevice> m_device;
    std::shared_ptr<IRHICommandQueue> m_command_queue;
    std::shared_ptr<IRHISwapChain> m_swap_chain;
    std::shared_ptr<IRHIMemoryManager> m_memory_manager;
    
    std::vector<std::shared_ptr<IRHICommandAllocator>> m_command_allocators;
    std::vector<std::shared_ptr<IRHICommandList>> m_command_lists;

    std::shared_ptr<IRHIRenderTargetManager> m_render_target_manager;
    std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> m_swapchain_RTs;

    std::shared_ptr<glTFRenderMaterialManager> m_material_manager;
    std::shared_ptr<glTFRenderMeshManager> m_mesh_manager;
    
    //std::vector<glTFRenderResourceFrameManager> m_frame_resource_managers;

    std::map<RendererInterface::ShaderHandle, std::shared_ptr<IRHIShader>> m_shaders;
    std::map<RendererInterface::RenderTargetHandle, std::shared_ptr<IRHITextureDescriptorAllocation>> m_render_targets;
    std::map<RendererInterface::RenderTargetHandle, RendererInterface::RenderTargetDesc> m_render_target_descs;
    RendererInterface::SwapchainLifecycleState m_swapchain_lifecycle_state{RendererInterface::SwapchainLifecycleState::UNINITIALIZED};

    unsigned m_last_requested_swapchain_width{0};
    unsigned m_last_requested_swapchain_height{0};
    unsigned m_last_observed_window_width{0};
    unsigned m_last_observed_window_height{0};
    unsigned m_resize_request_stable_frame_count{0};
    unsigned m_swapchain_resize_retry_countdown_frames{0};
    unsigned m_swapchain_resize_failure_count{0};
    unsigned m_swapchain_resize_last_failed_width{0};
    unsigned m_swapchain_resize_last_failed_height{0};
    unsigned m_swapchain_acquire_failure_count{0};
    unsigned m_swapchain_present_failure_count{0};

private:
    struct DeferredReleaseEntry
    {
        unsigned long long retire_frame{0};
        std::vector<std::shared_ptr<IRHIResource>> resources;
    };

    void SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState state, const char* reason = nullptr);
    bool ResizeWindowDependentRenderTargetsImpl(unsigned width, unsigned height, bool assume_gpu_idle);
    void AdvanceDeferredReleaseFrame();
    void FlushDeferredResourceReleases(bool force_release_all);
    void EnqueueResourceForDeferredRelease(const std::shared_ptr<IRHIResource>& resource);
    unsigned GetDeferredReleaseLatencyFrames() const;
    unsigned ComputeRetryCooldownFrames(unsigned failure_count) const;
    unsigned GetRetryLogPeriod() const;
    void UpdateResizeRequestStability(unsigned width, unsigned height);
    bool IsResizeRequestStableEnough(unsigned width, unsigned height, bool pending_retry_for_same_size) const;

    std::deque<DeferredReleaseEntry> m_deferred_release_entries;
    unsigned long long m_deferred_release_frame_index{0};
};
