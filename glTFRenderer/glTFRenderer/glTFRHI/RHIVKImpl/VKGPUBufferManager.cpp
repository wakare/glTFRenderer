#include "VKGPUBufferManager.h"

bool VKGPUBufferManager::InitMemoryManager(IRHIDevice& device, std::shared_ptr<IRHIMemoryAllocator> memory_allocator, unsigned maxDescriptorCount)
{
    return true;
}

bool VKGPUBufferManager::AllocateBufferMemory(IRHIDevice& device, const RHIBufferDesc& buffer_desc,
    std::shared_ptr<IRHIBufferAllocation>& out_buffer_allocation)
{
    return true;
}

bool VKGPUBufferManager::UploadBufferData(IRHIBufferAllocation& buffer_allocation, const void* data, size_t offset,
    size_t size)
{
    return true;
}

