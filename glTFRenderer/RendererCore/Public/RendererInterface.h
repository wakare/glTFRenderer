#pragma once

#include "Renderer.h"

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
    };

    class RenderGraph
    {
    public:
        bool RegisterRenderPass(RenderPassHandle render_pass_handle);
    };
}
