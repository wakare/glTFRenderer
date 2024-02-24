#pragma once
#include <vulkan/vulkan_core.h>

#include "glTFRHI/RHIInterface/IRHICommandQueue.h"

class VKCommandQueue : public IRHICommandQueue
{
public:
    VKCommandQueue();
    virtual ~VKCommandQueue() override;
    
    virtual bool InitCommandQueue(IRHIDevice& device) override;
    
    
protected:
    VkDevice logical_device;
    VkQueue graphics_queue;
};
