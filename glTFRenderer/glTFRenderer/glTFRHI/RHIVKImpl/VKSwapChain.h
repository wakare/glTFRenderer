#pragma once
#include <vulkan/vulkan_core.h>

#include "glTFRHI/RHIInterface/IRHISemaphore.h"
#include "glTFRHI/RHIInterface/IRHISwapChain.h"

class VKSwapChain : public IRHISwapChain
{
public:
    VKSwapChain();
    virtual ~VKSwapChain() override;

    virtual unsigned GetWidth() const override;
    virtual unsigned GetHeight() const override;
    
    virtual unsigned GetCurrentBackBufferIndex() override;
    virtual unsigned GetBackBufferCount() override;
    
    virtual bool InitSwapChain(IRHIFactory& factory, IRHIDevice& device, IRHICommandQueue& commandQueue, unsigned width, unsigned height, bool fullScreen, HWND hwnd) override;
    virtual bool AcquireNewFrame(IRHIDevice& device) override;
    virtual IRHISemaphore& GetAvailableFrameSemaphore() override;
    virtual bool Present(IRHICommandQueue& command_queue, IRHICommandList& command_list) override;
    
    VkImage GetSwapChainImageByIndex(unsigned index) const;
    
protected:
    unsigned m_frame_buffer_count;
    unsigned m_width {0};
    unsigned m_height {0};
    
    unsigned m_current_frame_index {0};
    
    VkDevice m_device {VK_NULL_HANDLE};
    VkSwapchainKHR m_swap_chain{VK_NULL_HANDLE};
    std::vector<VkImage> m_swap_chain_images;
    std::vector<std::shared_ptr<IRHISemaphore>> m_frame_available_semaphores;
};
