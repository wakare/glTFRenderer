#pragma once

#include "RenderInterfaceCommon.h"

class ResourceManager;

namespace RendererInterface
{
    class ResourceAllocator
    {
    public:
        bool Init(RenderDeviceDesc device);
        
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
