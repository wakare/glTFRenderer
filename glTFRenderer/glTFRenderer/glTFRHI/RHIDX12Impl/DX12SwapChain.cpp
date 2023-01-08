#include "DX12SwapChain.h"
#include <../glTFRenderer/glTFRHI/glTFRHIDX12.h>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32 1
#include <GLFW/glfw3native.h>

#include "DX12CommandQueue.h"
#include "DX12Factory.h"

DX12SwapChain::DX12SwapChain()
    : m_frameBufferCount(3)
    , m_swapChain(nullptr)
    , m_swapChainSampleDesc({})
{
}

bool DX12SwapChain::InitSwapChain(IRHIFactory& factory, IRHICommandQueue& commandQueue, unsigned width, unsigned height, bool fullScreen, glTFWindow& window)
{
    HWND hwnd = glfwGetWin32Window(window.GetRawWindow());
    
    // -- Create the Swap Chain (double/tripple buffering) -- //
    DXGI_MODE_DESC backBufferDesc = {}; // this is to describe our display mode
    backBufferDesc.Width = width; // buffer width
    backBufferDesc.Height = height; // buffer height
    backBufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // format of the buffer (rgba 32 bits, 8 bits for each chanel)
    
    // describe our multi-sampling. We are not multi-sampling, so we set the count to 1 (we need at least one sample of course)
    m_swapChainSampleDesc.Count = 1; // multisample count (no multisampling, so we just put 1, since we still need 1 sample)
    
    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = m_frameBufferCount; // number of buffers we have
    swapChainDesc.BufferDesc = backBufferDesc; // our back buffer description
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // this says the pipeline will render to this swap chain
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // dxgi will discard the buffer (data) after we call present
    swapChainDesc.OutputWindow = hwnd; // handle to our window
    swapChainDesc.SampleDesc = m_swapChainSampleDesc; // our multi-sampling description
    swapChainDesc.Windowed = !fullScreen; // set to true, then if in fullscreen must call SetFullScreenState with true for full screen to get uncapped fps

    auto dxFactory = dynamic_cast<DX12Factory&>(factory).GetFactory();
    auto dxCommandQueue = dynamic_cast<DX12CommandQueue&>(commandQueue).GetCommandQueue();
    IDXGISwapChain* tempSwapChain;
    dxFactory->CreateSwapChain(
        dxCommandQueue, // the queue will be flushed once the swap chain is created
        &swapChainDesc, // give it the swap chain description we created above
        &tempSwapChain // store the created swap chain in a temp IDXGISwapChain interface
        );
    
    m_swapChain = static_cast<IDXGISwapChain3*>(tempSwapChain);
    
    return true;
}
