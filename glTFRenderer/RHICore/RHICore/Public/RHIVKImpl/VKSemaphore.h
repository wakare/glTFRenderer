#pragma once
#include "VolkUtils.h"
#include "IRHISemaphore.h"

class VKSemaphore : public IRHISemaphore
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKSemaphore)

    virtual bool InitSemaphore(IRHIDevice& device) override;
    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
    VkSemaphore GetSemaphore() const;
    
protected:
    VkDevice m_device;
    VkSemaphore m_semaphore {VK_NULL_HANDLE};    
};
