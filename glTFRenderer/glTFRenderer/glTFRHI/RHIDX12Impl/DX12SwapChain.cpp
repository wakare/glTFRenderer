#include "DX12SwapChain.h"

#include <GLFW/glfw3.h>

#include "DX12CommandQueue.h"
#include "DX12ConverterUtils.h"
#include "DX12Factory.h"
#include "DX12Utils.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIResourceFactory.h"
#include "RenderWindow/glTFWindow.h"

unsigned DX12SwapChain::GetCurrentBackBufferIndex()
{
    return m_swap_chain->GetCurrentBackBufferIndex();
}

unsigned DX12SwapChain::GetBackBufferCount()
{
    return m_frame_buffer_count;
}

bool DX12SwapChain::InitSwapChain(IRHIFactory& factory, IRHIDevice& device, IRHICommandQueue& command_queue, const RHITextureDesc& swap_chain_buffer_desc, bool full_screen, HWND hwnd)
{
    m_swap_chain_buffer_desc = swap_chain_buffer_desc;
    
    // -- Create the Swap Chain (double/tripple buffering) -- //
    DXGI_MODE_DESC back_buffer_desc = {}; // this is to describe our display mode
    back_buffer_desc.Width = GetWidth(); // buffer width
    back_buffer_desc.Height = GetHeight(); // buffer height
    back_buffer_desc.Format = DX12ConverterUtils::ConvertToDXGIFormat(GetBackBufferFormat()); // format of the buffer (rgba 32 bits, 8 bits for each chanel)
    
    // describe our multi-sampling. We are not multi-sampling, so we set the count to 1 (we need at least one sample of course)
    m_swap_chain_sample_desc.Count = 1; // multisample count (no multisampling, so we just put 1, since we still need 1 sample)
    
    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};
    swap_chain_desc.BufferCount = m_frame_buffer_count; // number of buffers we have
    swap_chain_desc.BufferDesc = back_buffer_desc; // our back buffer description
    if (swap_chain_buffer_desc.GetUsage() & RUF_ALLOW_RENDER_TARGET)
    {
        swap_chain_desc.BufferUsage |= DXGI_USAGE_RENDER_TARGET_OUTPUT; // this says the pipeline will render to this swap chain
    }
    if (swap_chain_buffer_desc.GetUsage() & RUF_ALLOW_UAV)
    {
        swap_chain_desc.BufferUsage |= DXGI_USAGE_UNORDERED_ACCESS; // this says the pipeline will render to this swap chain
    }
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // dxgi will discard the buffer (data) after we call present
    swap_chain_desc.OutputWindow = hwnd; // handle to our window
    swap_chain_desc.SampleDesc = m_swap_chain_sample_desc; // our multi-sampling description
    swap_chain_desc.Windowed = !full_screen; // set to true, then if in fullscreen must call SetFullScreenState with true for full screen to get uncapped fps

    auto dx_factory = dynamic_cast<DX12Factory&>(factory).GetFactory();
    auto dx_command_queue = dynamic_cast<DX12CommandQueue&>(command_queue).GetCommandQueue();
    ComPtr<IDXGISwapChain> temp_swap_chain;
    dx_factory->CreateSwapChain(
        dx_command_queue, // the queue will be flushed once the swap chain is created
        &swap_chain_desc, // give it the swap chain description we created above
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
    THROW_IF_FAILED(m_swap_chain->Present(1, 0))
    dynamic_cast<DX12Fence&>(*m_fence).SignalWhenCommandQueueFinish(command_queue);
    
    return true;
}

bool DX12SwapChain::HostWaitPresentFinished(IRHIDevice& device)
{
    m_fence->HostWaitUtilSignaled();
    m_fence->ResetFence();
    
    return true;
}

bool DX12SwapChain::Release(glTFRenderResourceManager& resource_manager)
{
    auto factory = dynamic_cast<DX12Factory&>(resource_manager.GetFactory()).GetFactory();
    factory->MakeWindowAssociation(glTFWindow::Get().GetHWND(), DXGI_MWA_NO_ALT_ENTER);

    SAFE_RELEASE(m_swap_chain)

    return true;
}
