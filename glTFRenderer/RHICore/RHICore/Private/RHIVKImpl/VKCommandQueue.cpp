#include "VKCommandQueue.h"

#include "VKDevice.h"

bool VKCommandQueue::InitCommandQueue(IRHIDevice& device)
{
    logical_device = dynamic_cast<VKDevice&>(device).GetDevice();
    const unsigned graphics_queue_index = dynamic_cast<VKDevice&>(device).GetGraphicsQueueIndex();
    
    // TODO: Create graphics queue now
    vkGetDeviceQueue(logical_device, graphics_queue_index, 0, &graphics_queue);
    
    need_release = true;
    
    return true;
}

VkQueue VKCommandQueue::GetGraphicsQueue() const
{
    return graphics_queue;
}

bool VKCommandQueue::Release(IRHIMemoryManager& memory_manager)
{
    if (!need_release)
    {
        return true;
    }

    need_release = false;
    vkQueueWaitIdle(graphics_queue);
    
    return true;
}
