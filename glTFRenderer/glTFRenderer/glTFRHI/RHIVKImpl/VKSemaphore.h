#pragma once
#include "VolkUtils.h"
#include "glTFRHI/RHIInterface/IRHISemaphore.h"

class VKSemaphore : public IRHISemaphore
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKSemaphore)

    virtual bool InitSemaphore(IRHIDevice& device) override;
    virtual bool Release(glTFRenderResourceManager&) override;
    
    VkSemaphore GetSemaphore() const;
    
protected:
    VkDevice m_device;
    VkSemaphore m_semaphore {VK_NULL_HANDLE};    
};
