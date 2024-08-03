#pragma once

#include <vulkan/vulkan_core.h>
#include "glTFRHI/RHIInterface/IRHICommandQueue.h"

class VKCommandQueue : public IRHICommandQueue
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKCommandQueue)
    
    virtual bool InitCommandQueue(IRHIDevice& device) override;
    
    VkQueue GetGraphicsQueue() const;
    
protected:
    VkDevice logical_device {VK_NULL_HANDLE};
    VkQueue graphics_queue {VK_NULL_HANDLE};
};
