#include "RendererInterface.h"

#include "InternalResourceHandleTable.h"
#include "RenderPass.h"
#include "ResourceManager.h"
#include "RenderWindow/glTFWindow.h"

namespace RendererInterface
{
    RenderWindow::RenderWindow(const RenderWindowDesc& desc)
        : m_desc(desc)
        , m_handle(0)
        , m_hwnd(nullptr)
    {
        glTFWindow::Get().InitAndShowWindow();
        m_handle = InternalResourceHandleTable::Instance().RegisterWindow(*this);
        m_hwnd = glTFWindow::Get().GetHWND();
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

    void RenderWindow::TickWindow() const
    {
        glTFWindow::Get().UpdateWindow();
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
        return m_resource_manager->CreateShader(desc);
    }

    TextureHandle ResourceAllocator::CreateTexture(const TextureDesc& desc)
    {
        return 0;
    }

    RenderTargetHandle ResourceAllocator::CreateRenderTarget(const RenderTargetDesc& desc)
    {
        return m_resource_manager->CreateRenderTarget(desc);
    }

    RenderPassHandle ResourceAllocator::CreateRenderPass(const RenderPassDesc& desc)
    {
        std::shared_ptr<RenderPass> render_pass = std::make_shared<RenderPass>(desc);
        render_pass->InitRenderPass(*m_resource_manager);
        
        return InternalResourceHandleTable::Instance().RegisterRenderPass(render_pass);
    }

    RenderGraph::RenderGraph(ResourceAllocator& allocator, RenderWindow& window)
    {
    }

    bool RenderGraph::RegisterRenderPass(RenderPassHandle render_pass_handle)
    {
        
        return true;
    }

    bool RenderGraph::CompileRenderPassAndExecute()
    {
        return true;
    }
}

