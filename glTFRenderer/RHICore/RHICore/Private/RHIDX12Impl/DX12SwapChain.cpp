#include "DX12SwapChain.h"

#include <GLFW/glfw3.h>

#include "DX12CommandQueue.h"
#include "DX12ConverterUtils.h"
#include "DX12Factory.h"
#include "DX12Utils.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "RHIResourceFactory.h"
#include "RenderWindow/glTFWindow.h"

unsigned DX12SwapChain::GetCurrentBackBufferIndex()
{
    return m_swap_chain->GetCurrentBackBufferIndex();
}

unsigned DX12SwapChain::GetBackBufferCount()
{
    return m_frame_buffer_count;
}

bool DX12SwapChain::InitSwapChain(IRHIFactory& factory, IRHIDevice& device, IRHICommandQueue& command_queue, const RHITextureDesc& swap_chain_buffer_desc, const
                                  RHISwapChainDesc& swap_chain_desc)
{
    m_swap_chain_buffer_desc = swap_chain_buffer_desc;
    m_swap_chain_mode = swap_chain_desc.chain_mode;
    
    // -- Create the Swap Chain (double/tripple buffering) -- //
    DXGI_MODE_DESC back_buffer_desc = {}; // this is to describe our display mode
    back_buffer_desc.Width = GetWidth(); // buffer width
    back_buffer_desc.Height = GetHeight(); // buffer height
    back_buffer_desc.Format = DX12ConverterUtils::ConvertToDXGIFormat(GetBackBufferFormat()); // format of the buffer (rgba 32 bits, 8 bits for each chanel)
    
    // describe our multi-sampling. We are not multi-sampling, so we set the count to 1 (we need at least one sample of course)
    m_swap_chain_sample_desc.Count = 1; // multisample count (no multisampling, so we just put 1, since we still need 1 sample)
    
    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC dxgi_swap_chain_desc = {};
    dxgi_swap_chain_desc.BufferCount = m_frame_buffer_count; // number of buffers we have
    dxgi_swap_chain_desc.BufferDesc = back_buffer_desc; // our back buffer description
    if (swap_chain_buffer_desc.GetUsage() & RUF_ALLOW_RENDER_TARGET)
    {
        dxgi_swap_chain_desc.BufferUsage |= DXGI_USAGE_RENDER_TARGET_OUTPUT; // this says the pipeline will render to this swap chain
    }
    if (swap_chain_buffer_desc.GetUsage() & RUF_ALLOW_UAV)
    {
        dxgi_swap_chain_desc.BufferUsage |= DXGI_USAGE_UNORDERED_ACCESS; // this says the pipeline will render to this swap chain
    }
    dxgi_swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // dxgi will discard the buffer (data) after we call present
    dxgi_swap_chain_desc.OutputWindow = swap_chain_desc.hwnd; // handle to our window
    dxgi_swap_chain_desc.SampleDesc = m_swap_chain_sample_desc; // our multi-sampling description
    dxgi_swap_chain_desc.Windowed = !swap_chain_desc.full_screen; // set to true, then if in fullscreen must call SetFullScreenState with true for full screen to get uncapped fps
    if (m_swap_chain_mode == MAILBOX)
    {
        dxgi_swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    }

    auto dx_factory = dynamic_cast<DX12Factory&>(factory).GetFactory();
    auto dx_command_queue = dynamic_cast<DX12CommandQueue&>(command_queue).GetCommandQueue();
    ComPtr<IDXGISwapChain> temp_swap_chain;
    dx_factory->CreateSwapChain(
        dx_command_queue, // the queue will be flushed once the swap chain is created
        &dxgi_swap_chain_desc, // give it the swap chain description we created above
        &temp_swap_chain // store the created swap chain in a temp IDXGISwapChain interface
        );
    
    temp_swap_chain.As(&m_swap_chain);
    GLTF_CHECK(m_swap_chain);

    m_fence = RHIResourceFactory::CreateRHIResource<IRHIFence>();
    m_fence->InitFence(device);
    
    need_release = true;
    
    return true;
}

bool DX12SwapChain::AcquireNewFrame(IRHIDevice& device)
{
    return true;
}

IRHISemaphore& DX12SwapChain::GetAvailableFrameSemaphore()
{
    static RHISemaphoreNull _dummy_semaphore;
    return _dummy_semaphore;
}

bool DX12SwapChain::Present(IRHICommandQueue& command_queue, IRHICommandList& command_list)
{
    auto sync_interval = m_swap_chain_mode == VSYNC ? 1 : 0;
    UINT flag = m_swap_chain_mode == VSYNC ? 0 : DXGI_PRESENT_ALLOW_TEARING ;
    THROW_IF_FAILED(m_swap_chain->Present(sync_interval, flag))
    dynamic_cast<DX12Fence&>(*m_fence).SignalWhenCommandQueueFinish(command_queue);
    
    return true;
}

bool DX12SwapChain::HostWaitPresentFinished(IRHIDevice& device)
{
    m_fence->HostWaitUtilSignaled();
    m_fence->ResetFence();
    
    return true;
}

bool DX12SwapChain::Release(IRHIMemoryManager& memory_manager)
{
    SAFE_RELEASE(m_swap_chain)
    return true;
}
