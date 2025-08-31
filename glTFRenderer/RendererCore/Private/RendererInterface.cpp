#include "RendererInterface.h"

#include "InternalResourceHandleTable.h"
#include "RendererCommon.h"
#include "ResourceManager.h"

namespace RendererInterface
{
    RenderWindow::RenderWindow(const RenderWindowDesc& desc)
        : m_desc(desc)
        , m_handle(0)
        , m_hwnd(nullptr)
    {
        m_handle = s_internal_resource_handle_table.RegisterWindow(desc);
    }

    RenderWindowHandle RenderWindow::GetHandle() const
    {
        return m_handle;
    }

    unsigned RenderWindow::GetWidth() const
    {
        return m_desc.width;
    }

    unsigned RenderWindow::GetHeight() const
    {
        return m_desc.height;
    }

    HWND RenderWindow::GetHWND() const
    {
        return m_hwnd;
    }

    ResourceAllocator::ResourceAllocator(RenderDeviceDesc device)
    {
        if (!m_resource_manager)
        {
            m_resource_manager = std::make_shared<ResourceManager>();
            m_resource_manager->InitResourceManager(device);
        }
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

