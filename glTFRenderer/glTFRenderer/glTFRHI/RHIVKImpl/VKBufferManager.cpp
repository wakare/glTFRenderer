#include "VKBufferManager.h"

bool VKBufferManager::InitMemoryManager(IRHIDevice& device, std::shared_ptr<IRHIMemoryAllocator> memory_allocator, unsigned maxDescriptorCount)
{
    return true;
}

bool VKBufferManager::AllocateBufferMemory(IRHIDevice& device, const RHIBufferDesc& buffer_desc,
    std::shared_ptr<IRHIBufferAllocation>& out_buffer_allocation)
{
    return true;
}

bool VKBufferManager::UploadBufferData(IRHIBufferAllocation& buffer_allocation, const void* data, size_t offset,
    size_t size)
{
    return true;
}

bool VKBufferManager::AllocateTextureMemoryAndUpload(IRHIDevice& device, IRHICommandList& command_list,
    const RHITextureDesc& buffer_desc, std::shared_ptr<IRHITextureAllocation>& out_buffer_allocation)
{
    return false;
}
