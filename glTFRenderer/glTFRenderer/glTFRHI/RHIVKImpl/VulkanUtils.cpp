#include "VulkanUtils.h"

bool VulkanUtils::InitGUIContext(IRHIDevice& device, IRHIDescriptorHeap& descriptor_heap, unsigned back_buffer_count)
{
    return true;
}

bool VulkanUtils::WaitCommandQueueFinish(IRHICommandQueue& command_queue)
{
    return command_queue.WaitCommandQueue();
}

bool VulkanUtils::SupportRayTracing(IRHIDevice& device)
{
    return false;
}
