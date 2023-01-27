#pragma once
#include <memory>
#include <vector>

#include "../glTFRHI/RHIInterface/IRHICommandList.h"
#include "../glTFRHI/RHIInterface/IRHIDevice.h"
#include "../glTFRHI/RHIInterface/IRHIFence.h"
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
    
    IRHICommandAllocator& GetCurrentFrameCommandAllocator();
    IRHIFence& GetCurrentFrameFence();
        
private:
    // Exist one only
    std::shared_ptr<IRHIFactory> m_factory;
    std::shared_ptr<IRHIDevice> m_device;
    std::shared_ptr<IRHISwapChain> m_swapchain;
    std::shared_ptr<IRHICommandQueue> m_commandQueue;
    std::shared_ptr<IRHICommandList> m_commandList;
    
    // Exist one per frame
    std::vector<std::shared_ptr<IRHICommandAllocator>> m_commandAllocators;
    std::vector<std::shared_ptr<IRHIFence>> m_fences;
    
    unsigned m_currentFrameIndex;
};
