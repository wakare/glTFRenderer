#include "VKCommandQueue.h"

#include "VKDevice.h"

VKCommandQueue::VKCommandQueue()
    : logical_device(VK_NULL_HANDLE)
    , graphics_queue(VK_NULL_HANDLE)
{
}

VKCommandQueue::~VKCommandQueue()
{
}

bool VKCommandQueue::InitCommandQueue(IRHIDevice& device)
{
    logical_device = dynamic_cast<VKDevice&>(device).GetDevice();
    const unsigned graphics_queue_index = dynamic_cast<VKDevice&>(device).GetGraphicsQueueIndex();
    
    // TODO: Create graphics queue now
    vkGetDeviceQueue(logical_device, graphics_queue_index, 0, &graphics_queue);
    
    return true;
}
