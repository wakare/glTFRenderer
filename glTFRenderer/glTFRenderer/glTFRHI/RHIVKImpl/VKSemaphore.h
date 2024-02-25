#pragma once
#include <vulkan/vulkan_core.h>

#include "glTFRHI/RHIInterface/IRHISemaphore.h"

class VKSemaphore : public IRHISemaphore
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKSemaphore)
    
    bool InitSemaphore(VkSemaphore semaphore);
    VkSemaphore GetSemaphore() const;
    
protected:
    VkSemaphore m_semaphore { VK_NULL_HANDLE};    
};
