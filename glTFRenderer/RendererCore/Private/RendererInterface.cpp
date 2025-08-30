#include "RendererInterface.h"

#include "ResourceManager.h"
#include "RendererCommon.h"

namespace RendererInterface
{
    bool ResourceAllocator::Init(RenderDeviceDesc device)
    {
        if (!m_resource_manager)
        {
            m_resource_manager = std::make_shared<ResourceManager>();
            return m_resource_manager->InitResourceManager(device);
        }
        
        // Can not init twice
        GLTF_CHECK(false);
        return false;
    }

    ShaderHandle ResourceAllocator::CreateShader(const ShaderDesc& desc)
    {
        return true;
    }

    TextureHandle ResourceAllocator::CreateTexture(const TextureDesc& desc)
    {
        return true;
    }

    RenderTargetHandle ResourceAllocator::CreateRenderTarget(const RenderTargetDesc& desc)
    {
        return true;
    }

    RenderPassHandle ResourceAllocator::CreateRenderPass(const RenderPassDesc& desc)
    {
        return true;
    }
}

