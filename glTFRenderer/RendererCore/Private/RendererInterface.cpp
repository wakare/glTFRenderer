#include "RendererInterface.h"

#include <filesystem>

#include "InternalResourceHandleTable.h"
#include "RenderPass.h"
#include "ResourceManager.h"
#include "RHIUtils.h"
#include "RenderWindow/glTFWindow.h"
#include "RHIInterface/IRHIDescriptorManager.h"
#include "RHIInterface/IRHISwapChain.h"

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

    IRHITextureDescriptorAllocation& ResourceAllocator::GetCurrentSwapchainRT()
    {
        return m_resource_manager->GetCurrentSwapchainRT();
    }

    IRHICommandList& ResourceAllocator::GetCommandListForRecordPassCommand(RenderPassHandle pass) const
    {
        return m_resource_manager->GetCommandListForRecordPassCommand(pass);
    }

    IRHICommandList& ResourceAllocator::GetCommandListForExecution() const
    {
        return m_resource_manager->GetCommandListForExecution();
    }

    IRHICommandQueue& ResourceAllocator::GetCommandQueue() const
    {
        return m_resource_manager->GetCommandQueue();
    }

    IRHISwapChain& ResourceAllocator::GetCurrentSwapchain()
    {
        return m_resource_manager->GetSwapChain();
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
        // find final color output and copy to swapchain buffer, only debug logic
        RenderTargetHandle final_color_output_handle = NULL_HANDLE; 
        for (auto render_graph_node_handle : m_render_graph_node_handles)
        {
            auto& render_graph_node = m_render_graph_nodes[render_graph_node_handle];
            for (const auto& render_target_info : render_graph_node.draw_info.render_target_resources)
            {
                if (render_target_info.second.usage == COLOR)
                {
                    final_color_output_handle = render_target_info.first;
                }
            }
        }

        auto final_color_output = InternalResourceHandleTable::Instance().GetRenderTarget(final_color_output_handle);
        
        m_window.RegisterTickCallback([this, final_color_output]()
        {
            for (auto render_graph_node : m_render_graph_node_handles)
            {
                ExecuteRenderNode(render_graph_node);
            }

            if (!m_render_graph_node_handles.empty())
            {
                auto src = final_color_output->m_source;
                auto dst = m_resource_allocator.GetCurrentSwapchainRT().m_source;

                RHICopyTextureInfo copy_info{};
                copy_info.copy_width = m_window.GetWidth();
                copy_info.copy_height = m_window.GetHeight();
                copy_info.dst_mip_level = 0;
                copy_info.src_mip_level = 0;
                copy_info.dst_x = 0;
                copy_info.dst_y = 0;
                RHIUtilInstanceManager::Instance().CopyTexture(m_resource_allocator.GetCommandListForRecordPassCommand(NULL_HANDLE), *dst, *src, copy_info);
                
                Present();
            }
        });

        return true;
    }

    void RenderGraph::ExecuteRenderNode(RenderGraphNodeHandle render_graph_node_handle)
    {
        auto& render_graph_node_desc = m_render_graph_nodes[render_graph_node_handle];
        auto& command_list = m_resource_allocator.GetCommandListForRecordPassCommand(render_graph_node_desc.render_pass_handle);
        auto render_pass = InternalResourceHandleTable::Instance().GetRenderPass(render_graph_node_desc.render_pass_handle);

        RHIUtilInstanceManager::Instance().SetRootSignature(command_list, render_pass->GetRootSignature(), render_pass->GetPipelineStateObject(), RendererInterfaceRHIConverter::ConvertToRHIPipelineType(render_pass->GetRenderPassType()));
        RHIUtilInstanceManager::Instance().SetPrimitiveTopology( command_list, RHIPrimitiveTopologyType::TRIANGLELIST);

        RHIViewportDesc viewport{};
        viewport.width = m_window.GetWidth();
        viewport.height = m_window.GetHeight();
        viewport.min_depth = 0.f;
        viewport.max_depth = 1.f;
        viewport.top_left_x = 0.f;
        viewport.top_left_y = 0.f;
        RHIUtilInstanceManager::Instance().SetViewport(command_list, viewport);

        const RHIScissorRectDesc scissor_rect =
            {
            (unsigned)viewport.top_left_x,
            (unsigned)viewport.top_left_y,
            (unsigned)(viewport.top_left_x + viewport.width),
            (unsigned)(viewport.top_left_y + viewport.height)
        }; 
        RHIUtilInstanceManager::Instance().SetScissorRect(command_list, scissor_rect);
        
        RHIBeginRenderingInfo begin_rendering_info{};
        
        // render target binding
        for (const auto& render_target_info : render_graph_node_desc.draw_info.render_target_resources)
        {
            auto render_target = InternalResourceHandleTable::Instance().GetRenderTarget(render_target_info.first);
            begin_rendering_info.m_render_targets.push_back(render_target.get());
        }

        RHIUtilInstanceManager::Instance().BeginRendering(command_list, begin_rendering_info);

        
        const auto& draw_info = render_graph_node_desc.draw_info;
        switch (draw_info.execute_command.type) {
        case ExecuteCommandType::DRAW_VERTEX_COMMAND:
            RHIUtilInstanceManager::Instance().DrawInstanced(command_list,
                draw_info.execute_command.parameter.draw_vertex_command_parameter.vertex_count,
                1,
                draw_info.execute_command.parameter.draw_vertex_command_parameter.start_vertex_location,
                0);
            break;
        case ExecuteCommandType::DRAW_VERTEX_INSTANCING_COMMAND:
            RHIUtilInstanceManager::Instance().DrawInstanced(command_list,
                draw_info.execute_command.parameter.draw_vertex_instance_command_parameter.vertex_count,
                draw_info.execute_command.parameter.draw_vertex_instance_command_parameter.instance_count,
                draw_info.execute_command.parameter.draw_vertex_instance_command_parameter.start_vertex_location,
                draw_info.execute_command.parameter.draw_vertex_instance_command_parameter.start_instance_location);
            break;
        case ExecuteCommandType::DRAW_INDEXED_COMMAND:
            break;
        case ExecuteCommandType::DRAW_INDEXED_INSTANCING_COMMAND:
            break;
        case ExecuteCommandType::COMPUTE_DISPATCH_COMMAND:
            break;
        case ExecuteCommandType::RAY_TRACING_COMMAND:
            break;
        }

        RHIUtilInstanceManager::Instance().EndRendering(command_list);
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

    void RenderGraph::Present()
    {
        auto& command_list = m_resource_allocator.GetCommandListForRecordPassCommand(NULL_HANDLE);
        m_resource_allocator.GetCurrentSwapchainRT().m_source->Transition(command_list, RHIResourceStateType::STATE_PRESENT);

        RHIExecuteCommandListContext context;
        context.wait_infos.push_back({&m_resource_allocator.GetCurrentSwapchain().GetAvailableFrameSemaphore(), RHIPipelineStage::COLOR_ATTACHMENT_OUTPUT});
        context.sign_semaphores.push_back(&command_list.GetSemaphore());
    
        RHIUtilInstanceManager::Instance().Present(m_resource_allocator.GetCurrentSwapchain(), m_resource_allocator.GetCommandQueue(), m_resource_allocator.GetCommandListForRecordPassCommand(NULL_HANDLE));
        CloseCurrentCommandListAndExecute({}, false);
    }
}

