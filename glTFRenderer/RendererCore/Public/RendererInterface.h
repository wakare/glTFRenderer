#pragma once

#include <set>

#include "Renderer.h"

class RenderPass;
class ResourceManager;

namespace RendererInterface
{
    class RenderWindow
    {
    public:
        RenderWindow(const RenderWindowDesc& desc);
        RenderWindowHandle GetHandle() const;
        unsigned GetWidth() const;
        unsigned GetHeight() const;
        HWND GetHWND() const;
        void TickWindow() const;
        
    protected:
        RenderWindowDesc m_desc;
        RenderWindowHandle m_handle;
        HWND m_hwnd;
    };
    
    class ResourceAllocator
    {
    public:
        ResourceAllocator(RenderDeviceDesc device);
        
        ShaderHandle        CreateShader(const ShaderDesc& desc);
        TextureHandle       CreateTexture(const TextureDesc& desc);
        RenderTargetHandle  CreateRenderTarget(const RenderTargetDesc& desc);
        RenderPassHandle    CreateRenderPass(const RenderPassDesc& desc);
        
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
        std::vector<RenderGraphNodeDesc> m_render_graph_nodes;
        std::set<RenderGraphNodeHandle> m_render_graph_node_handles;
    };
}
