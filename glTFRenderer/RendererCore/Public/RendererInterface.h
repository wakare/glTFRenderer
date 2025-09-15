#pragma once

#include <functional>
#include <set>

#include "Renderer.h"

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
    
    class ResourceAllocator
    {
    public:
        ResourceAllocator(RenderDeviceDesc device);
        unsigned            GetCurrentBackBufferIndex() const;
        
        ShaderHandle        CreateShader(const ShaderDesc& desc);
        TextureHandle       CreateTexture(const TextureDesc& desc);
        RenderTargetHandle  CreateRenderTarget(const RenderTargetDesc& desc);
        RenderPassHandle    CreateRenderPass(const RenderPassDesc& desc);

        IRHICommandList&    GetCommandListForRecordPassCommand(RenderPassHandle pass = NULL_HANDLE) const;
        IRHICommandQueue&   GetCommandQueue() const;
        IRHISwapChain&      GetCurrentSwapchain();
        
        IRHITextureDescriptorAllocation& GetCurrentSwapchainRT();
        
    protected:
        std::shared_ptr<ResourceManager> m_resource_manager;
        std::map<RenderPassHandle, std::shared_ptr<RenderPass>> m_render_passes;
    };

    class RenderGraph
    {
    public:
        RenderGraph(ResourceAllocator& allocator, RenderWindow& window);
        
        RenderGraphNodeHandle CreateRenderGraphNode(const RenderGraphNodeDesc& render_graph_node_desc);
        
        bool RegisterRenderGraphNode(RenderGraphNodeHandle render_graph_node_handle);
        bool RemoveRenderGraphNode(RenderGraphNodeHandle render_graph_node_handle);
        
        bool CompileRenderPassAndExecute();

    protected:
        void ExecuteRenderNode(IRHICommandList& command_list, RenderGraphNodeHandle render_graph_node_handle);
        void CloseCurrentCommandListAndExecute(IRHICommandList& command_list, const RHIExecuteCommandListContext& context, bool wait);
        void Present(IRHICommandList& command_list);
        
        ResourceAllocator& m_resource_allocator;
        RenderWindow& m_window;
        
        std::vector<RenderGraphNodeDesc> m_render_graph_nodes;
        std::set<RenderGraphNodeHandle> m_render_graph_node_handles;
    };
}
