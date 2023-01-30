#pragma once
#include <memory>
#include <vector>

#include "../glTFRHI/RHIInterface/IRHICommandList.h"
#include "../glTFRHI/RHIInterface/IRHIDevice.h"
#include "../glTFRHI/RHIInterface/IRHIFence.h"
#include "../glTFRHI/RHIInterface/IRHIRenderTarget.h"
#include "../glTFRHI/RHIInterface/IRHISwapChain.h"

class glTFWindow;

// Hold all rhi resource
class glTFRenderResourceManager
{
public:
    glTFRenderResourceManager();
    
    bool InitResourceManager(glTFWindow& window);

    IRHIFactory& GetFactory();
    IRHIDevice& GetDevice();
    IRHISwapChain& GetSwapchain();
    IRHICommandQueue& GetCommandQueue();
    IRHICommandList& GetCommandList();
    IRHIRenderTargetManager& GetRenderTargetManager();
    
    IRHICommandAllocator& GetCurrentFrameCommandAllocator();
    IRHIFence& GetCurrentFrameFence();
    IRHIRenderTarget& GetCurrentFrameSwapchainRT();

    unsigned GetCurrentBackBufferIndex() const {return m_currentBackBufferIndex; }
    void UpdateFrameIndex() { m_currentBackBufferIndex = m_swapchain->GetCurrentBackBufferIndex(); }
    
private:
    std::shared_ptr<IRHIFactory> m_factory;
    std::shared_ptr<IRHIDevice> m_device;
    std::shared_ptr<IRHICommandQueue> m_commandQueue;
    std::shared_ptr<IRHISwapChain> m_swapchain;
    std::vector<std::shared_ptr<IRHICommandAllocator>> m_commandAllocators;
    std::shared_ptr<IRHICommandList> m_commandList;
    std::vector<std::shared_ptr<IRHIFence>> m_fences;
    std::shared_ptr<IRHIRenderTargetManager> m_renderTargetManager;
    std::vector<std::shared_ptr<IRHIRenderTarget>> m_swapchainRTs;
    
    unsigned m_currentBackBufferIndex;
};
