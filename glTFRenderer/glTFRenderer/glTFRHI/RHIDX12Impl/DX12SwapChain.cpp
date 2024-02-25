#include "DX12SwapChain.h"

#include <GLFW/glfw3.h>

#include "DX12CommandQueue.h"
#include "DX12ConverterUtils.h"
#include "DX12Factory.h"
#include "DX12Utils.h"
#include "../../glTFWindow/glTFWindow.h"

DX12SwapChain::DX12SwapChain()
    : m_frame_buffer_count(3)
    , m_width(0)
    , m_height(0)
    , m_swap_chain(nullptr)
    , m_swap_chain_sample_desc({})
{
}

DX12SwapChain::~DX12SwapChain()
{
    SAFE_RELEASE(m_swapChain)
}

unsigned DX12SwapChain::GetWidth()
{
    return m_width; 
}

unsigned DX12SwapChain::GetHeight()
{
    return m_height;
}

unsigned DX12SwapChain::GetCurrentBackBufferIndex()
{
    return m_swap_chain->GetCurrentBackBufferIndex();
}

unsigned DX12SwapChain::GetBackBufferCount()
{
    return m_frame_buffer_count;
}

bool DX12SwapChain::InitSwapChain(IRHIFactory& factory, IRHIDevice& device, IRHICommandQueue& commandQueue, unsigned width, unsigned height, bool fullScreen, HWND hwnd)
{
    m_width = width;
    m_height = height;
    
    // -- Create the Swap Chain (double/tripple buffering) -- //
    DXGI_MODE_DESC backBufferDesc = {}; // this is to describe our display mode
    backBufferDesc.Width = width; // buffer width
    backBufferDesc.Height = height; // buffer height
    backBufferDesc.Format = DX12ConverterUtils::ConvertToDXGIFormat(m_back_buffer_format); // format of the buffer (rgba 32 bits, 8 bits for each chanel)
    
    // describe our multi-sampling. We are not multi-sampling, so we set the count to 1 (we need at least one sample of course)
    m_swap_chain_sample_desc.Count = 1; // multisample count (no multisampling, so we just put 1, since we still need 1 sample)
    
    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = m_frame_buffer_count; // number of buffers we have
    swapChainDesc.BufferDesc = backBufferDesc; // our back buffer description
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // this says the pipeline will render to this swap chain
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // dxgi will discard the buffer (data) after we call present
    swapChainDesc.OutputWindow = hwnd; // handle to our window
    swapChainDesc.SampleDesc = m_swap_chain_sample_desc; // our multi-sampling description
    swapChainDesc.Windowed = !fullScreen; // set to true, then if in fullscreen must call SetFullScreenState with true for full screen to get uncapped fps

    auto dxFactory = dynamic_cast<DX12Factory&>(factory).GetFactory();
    auto dxCommandQueue = dynamic_cast<DX12CommandQueue&>(commandQueue).GetCommandQueue();
    IDXGISwapChain* tempSwapChain;
    dxFactory->CreateSwapChain(
        dxCommandQueue, // the queue will be flushed once the swap chain is created
        &swapChainDesc, // give it the swap chain description we created above
        &tempSwapChain // store the created swap chain in a temp IDXGISwapChain interface
        );
    
    m_swap_chain = static_cast<IDXGISwapChain3*>(tempSwapChain);
    
    return true;
}

bool DX12SwapChain::AcquireNewFrame(IRHIDevice& device)
{
    return true;
}

IRHISemaphore& DX12SwapChain::GetAvailableFrameSemaphore()
{
    static IRHISemaphore _dummy_semaphore;
    return _dummy_semaphore;
}

bool DX12SwapChain::Present(IRHICommandQueue& command_queue)
{
    THROW_IF_FAILED(m_swap_chain->Present(0, 0))
    return true;
}
