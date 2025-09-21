#pragma once

#include <functional>
#include <set>

#include "Renderer.h"

class IRHIBufferDescriptorAllocation;
class IRHIDescriptorManager;
class IRHIDevice;
class IRHISwapChain;
class IRHITextureDescriptorAllocation;
class IRHICommandList;
class IRHICommandQueue;
struct RHIExecuteCommandListContext;
class RenderPass;
class ResourceManager;

namespace RendererInterface
{
    class RenderWindow
    {
    public:
        typedef std::function<void()> RenderWindowTickCallback;
        
        RenderWindow(const RenderWindowDesc& desc);
        RenderWindowHandle GetHandle() const;
        unsigned GetWidth() const;
        unsigned GetHeight() const;
        HWND GetHWND() const;
        void TickWindow() const;

        void RegisterTickCallback(const RenderWindowTickCallback& callback);
        
    protected:
        RenderWindowDesc m_desc;
        RenderWindowHandle m_handle;
        HWND m_hwnd;
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
        BufferHandle        CreateBuffer(const BufferDesc& desc);
        RenderTargetHandle  CreateRenderTarget(const RenderTargetDesc& desc);
        RenderPassHandle    CreateRenderPass(const RenderPassDesc& desc);

        IRHIDevice&         GetDevice() const;
        IRHICommandQueue&   GetCommandQueue() const;
        IRHISwapChain&      GetCurrentSwapchain();
        IRHICommandList&    GetCommandListForRecordPassCommand(RenderPassHandle pass = NULL_HANDLE) const;
        IRHIDescriptorManager& GetDescriptorManager() const;
        
        IRHITextureDescriptorAllocation& GetCurrentSwapchainRT();

        void UploadBufferData(BufferHandle handle, const BufferUploadDesc& upload_desc);
        
    protected:
        std::shared_ptr<ResourceManager> m_resource_manager;
        std::map<RenderPassHandle, std::shared_ptr<RenderPass>> m_render_passes;
    };

    class RenderGraph
    {
    public:
        RenderGraph(ResourceOperator& allocator, RenderWindow& window);
        
        RenderGraphNodeHandle CreateRenderGraphNode(const RenderGraphNodeDesc& render_graph_node_desc);
        
        bool RegisterRenderGraphNode(RenderGraphNodeHandle render_graph_node_handle);
        bool RemoveRenderGraphNode(RenderGraphNodeHandle render_graph_node_handle);
        
        bool CompileRenderPassAndExecute();

    protected:
        void ExecuteRenderGraphNode(IRHICommandList& command_list, RenderGraphNodeHandle render_graph_node_handle);
        void CloseCurrentCommandListAndExecute(IRHICommandList& command_list, const RHIExecuteCommandListContext& context, bool wait);
        void Present(IRHICommandList& command_list);
        
        ResourceOperator& m_resource_allocator;
        RenderWindow& m_window;
        
        std::vector<RenderGraphNodeDesc> m_render_graph_nodes;
        std::set<RenderGraphNodeHandle> m_render_graph_node_handles;

        std::map<BufferHandle, std::shared_ptr<IRHIBufferDescriptorAllocation>> m_buffer_descriptors;
    };
}
