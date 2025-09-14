#include "RendererInterface.h"

#include "InternalResourceHandleTable.h"
#include "RenderPass.h"
#include "ResourceManager.h"
#include "RHIUtils.h"
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

    void RenderWindow::RegisterTickCallback(const RenderWindowTickCallback& callback)
    {
        glTFWindow::Get().SetTickCallback(callback);
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

    unsigned ResourceAllocator::GetCurrentBackBufferIndex() const
    {
        return m_resource_manager->GetCurrentBackBufferIndex();
    }

    IRHICommandList& ResourceAllocator::GetCommandListForRecord() const
    {
        return m_resource_manager->GetCommandListForRecord();
    }

    IRHICommandList& ResourceAllocator::GetCommandListForExecution() const
    {
        return m_resource_manager->GetCommandListForExecution();
    }

    IRHICommandQueue& ResourceAllocator::GetCommandQueue() const
    {
        return m_resource_manager->GetCommandQueue();
    }

    RenderGraph::RenderGraph(ResourceAllocator& allocator, RenderWindow& window)
        : m_resource_allocator(allocator)
        , m_window(window)
    {
        
    }

    RenderGraphNodeHandle RenderGraph::CreateRenderGraphNode(const RenderGraphNodeDesc& render_graph_node_desc)
    {
        auto result = m_render_graph_nodes.size();
        m_render_graph_nodes.push_back(render_graph_node_desc);
        return result;
    }

    bool RenderGraph::RegisterRenderGraphNode(RenderGraphNodeHandle render_graph_node_handle)
    {
        GLTF_CHECK(!m_render_graph_node_handles.contains(render_graph_node_handle));
        m_render_graph_node_handles.insert(render_graph_node_handle);
        return true;
    }

    bool RenderGraph::RemoveRenderGraphNode(RenderGraphNodeHandle render_graph_node_handle)
    {
        GLTF_CHECK(m_render_graph_node_handles.contains(render_graph_node_handle));
        m_render_graph_node_handles.erase(render_graph_node_handle);
        return true;
    }

    bool RenderGraph::CompileRenderPassAndExecute()
    {
        m_window.RegisterTickCallback([this]()
        {
            auto& command_list = m_resource_allocator.GetCommandListForRecord();
            
            RHIExecuteCommandListContext command_list_context{};
            CloseCurrentCommandListAndExecute(command_list_context, false);
        });

        return true;
    }

    void RenderGraph::CloseCurrentCommandListAndExecute(const RHIExecuteCommandListContext& context, bool wait)
    {
        auto& command_list = m_resource_allocator.GetCommandListForExecution();
        auto& command_queue = m_resource_allocator.GetCommandQueue();
        
        GLTF_CHECK(RHIUtilInstanceManager::Instance().ExecuteCommandList(command_list, command_queue, context));
        if (wait)
        {
            RHIUtilInstanceManager::Instance().WaitCommandListFinish(command_list);
        }
    }
}

