#pragma once
#include <vulkan/vulkan_core.h>

#include "glTFRHI/RHIInterface/IRHISemaphore.h"

class VKSemaphore : public IRHISemaphore
{
public:
    VKSemaphore() = default;
    virtual ~VKSemaphore() override;
    DECLARE_NON_COPYABLE(VKSemaphore)

    virtual bool InitSemaphore(IRHIDevice& device) override;
    
    VkSemaphore GetSemaphore() const;
    
protected:
    VkDevice m_device;
    VkSemaphore m_semaphore {VK_NULL_HANDLE};    
};
