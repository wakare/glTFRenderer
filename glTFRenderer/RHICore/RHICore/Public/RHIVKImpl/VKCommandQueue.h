#pragma once

#include "VolkUtils.h"
#include "IRHICommandQueue.h"

class VKCommandQueue : public IRHICommandQueue
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKCommandQueue)
    
    virtual bool InitCommandQueue(IRHIDevice& device) override;
    
    VkQueue GetGraphicsQueue() const;
    
    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
protected:
    VkDevice logical_device {VK_NULL_HANDLE};
    VkQueue graphics_queue {VK_NULL_HANDLE};
};
