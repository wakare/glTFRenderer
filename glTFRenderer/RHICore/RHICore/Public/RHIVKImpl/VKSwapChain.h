#pragma once
#include "VolkUtils.h"
#include "IRHISemaphore.h"
#include "IRHISwapChain.h"

class IRHITexture;

class VKSwapChain : public IRHISwapChain
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKSwapChain)

    virtual unsigned GetCurrentBackBufferIndex() override;
    virtual unsigned GetBackBufferCount() override;
    
    virtual bool InitSwapChain(IRHIFactory& factory, IRHIDevice& device, IRHICommandQueue& commandQueue, const RHITextureDesc& swap_chain_buffer_desc, const
                               RHISwapChainDesc& swap_chain_desc) override;
    virtual bool AcquireNewFrame(IRHIDevice& device) override;
    
    virtual IRHISemaphore& GetAvailableFrameSemaphore() override;
    virtual bool Present(IRHICommandQueue& command_queue, IRHICommandList& command_list) override;
    virtual bool HostWaitPresentFinished(IRHIDevice& device) override;
    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
    std::shared_ptr<IRHITexture> GetSwapChainImageByIndex(unsigned index) const;
    
protected:
    unsigned m_frame_buffer_count {3};
    unsigned m_current_frame_index {0};
    unsigned m_image_index {0};
    uint64_t m_present_id_count {0};
    
    VkDevice m_device {VK_NULL_HANDLE};
    VkSwapchainKHR m_swap_chain{VK_NULL_HANDLE};
    std::vector<std::shared_ptr<IRHITexture>> m_swap_chain_textures;
    std::vector<std::shared_ptr<IRHISemaphore>> m_frame_available_semaphores;
    std::map<unsigned, std::shared_ptr<IRHISemaphore>> m_frame_semaphore_wait_infos;
};
