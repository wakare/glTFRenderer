#include "ResourceManager.h"

#include "InternalResourceHandleTable.h"
#include "RHIResourceFactoryImpl.hpp"
#include "RHIInterface/IRHICommandAllocator.h"
#include "RHIInterface/IRHICommandList.h"
#include "RHIInterface/IRHIRenderTargetManager.h"
#include "RHIInterface/IRHISwapChain.h"

#define EXIT_WHEN_FALSE(x) if (!(x)) {assert(false); return false;}

bool ResourceManager::InitResourceManager(const RendererInterface::RenderDeviceDesc& desc)
{
    m_factory = RHIResourceFactory::CreateRHIResource<IRHIFactory>();
    EXIT_WHEN_FALSE(m_factory->InitFactory())  
    
    m_device = RHIResourceFactory::CreateRHIResource<IRHIDevice>();
    EXIT_WHEN_FALSE(m_device->InitDevice(*m_factory))
    
    m_command_queue = RHIResourceFactory::CreateRHIResource<IRHICommandQueue>();
    EXIT_WHEN_FALSE(m_command_queue->InitCommandQueue(*m_device))

    const auto& render_window = RendererInterface::InternalResourceHandleTable::Instance().GetRenderWindow(desc.window);
    
    m_swap_chain = RHIResourceFactory::CreateRHIResource<IRHISwapChain>();
    RHITextureDesc swap_chain_texture_desc("swap_chain_back_buffer",
        render_window.GetWidth(), render_window.GetHeight(),
        RHIDataFormat::R8G8B8A8_UNORM,
        static_cast<RHIResourceUsageFlags>(RUF_ALLOW_RENDER_TARGET | RUF_TRANSFER_DST),
        {
            .clear_format = RHIDataFormat::R8G8B8A8_UNORM,
            .clear_color = {0.0f, 0.0f, 0.0f, 0.0f}
        });

    RHISwapChainDesc swap_chain_desc;
    swap_chain_desc.hwnd = render_window.GetHWND();
    swap_chain_desc.chain_mode = VSYNC;
    swap_chain_desc.full_screen = false;
    EXIT_WHEN_FALSE(m_swap_chain->InitSwapChain(*m_factory, *m_device, *m_command_queue, swap_chain_texture_desc, swap_chain_desc ))

    m_memory_manager = RHIResourceFactory::CreateRHIResource<IRHIMemoryManager>();
    EXIT_WHEN_FALSE(m_memory_manager->InitMemoryManager(*m_device, *m_factory,
            {
            256,
            64,
            64
            }))
    
    m_command_allocators.resize(desc.back_buffer_count);
    m_command_lists.resize(desc.back_buffer_count);
    m_command_list_record_state.resize(desc.back_buffer_count);
    
    for (size_t i = 0; i < desc.back_buffer_count; ++i)
    {
        m_command_allocators[i] = RHIResourceFactory::CreateRHIResource<IRHICommandAllocator>();
        m_command_allocators[i]->InitCommandAllocator(*m_device, RHICommandAllocatorType::DIRECT);
        
        m_command_lists[i] = RHIResourceFactory::CreateRHIResource<IRHICommandList>();
        m_command_lists[i]->InitCommandList(*m_device, *m_command_allocators[i]);
        m_command_list_record_state[i] = false;
    }
    
    m_render_target_manager = RHIResourceFactory::CreateRHIResource<IRHIRenderTargetManager>();
    m_render_target_manager->InitRenderTargetManager(*m_device, 100);

    RHITextureClearValue clear_value
    {
        RHIDataFormat::R8G8B8A8_UNORM_SRGB,
        {0.0f, 0.0f, 0.0f, 0.0f}
    };

    m_swapchain_RTs = m_render_target_manager->CreateRenderTargetFromSwapChain(*m_device, *m_memory_manager, *m_swap_chain, clear_value);

    for (unsigned i = 0; i < desc.back_buffer_count; ++i)
    {
        auto depth_texture = m_render_target_manager->CreateRenderTarget(
            *m_device, *m_memory_manager,
            RHITextureDesc::MakeDepthTextureDesc(render_window.GetWidth(), render_window.GetHeight()), {
                .type = RHIRenderTargetType::DSV,
                .format = RHIDataFormat::D32_FLOAT
            });
    }
    
    //m_frame_resource_managers.resize(desc.back_buffer_count);

    return true;
}
