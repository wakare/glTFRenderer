#include "VKMemoryManager.h"

bool VKMemoryManager::InitMemoryManager(IRHIDevice& device, std::shared_ptr<IRHIMemoryAllocator> memory_allocator, const RHIMemoryManagerDescriptorMaxCapacity&
                                        max_descriptor_capacity)
{
    return true;
}

bool VKMemoryManager::AllocateBufferMemory(IRHIDevice& device, const RHIBufferDesc& buffer_desc,
    std::shared_ptr<IRHIBufferAllocation>& out_buffer_allocation)
{
    return true;
}

bool VKMemoryManager::UploadBufferData(IRHIBufferAllocation& buffer_allocation, const void* data, size_t offset,
    size_t size)
{
    return true;
}

bool VKMemoryManager::AllocateTextureMemory(IRHIDevice& device, const RHITextureDesc& buffer_desc,
    std::shared_ptr<IRHITextureAllocation>& out_buffer_allocation)
{
    return false;
}

bool VKMemoryManager::AllocateTextureMemoryAndUpload(IRHIDevice& device, IRHICommandList& command_list,
                                                     const RHITextureDesc& buffer_desc, std::shared_ptr<IRHITextureAllocation>& out_buffer_allocation)
{
    return false;
}

bool VKMemoryManager::CleanAllocatedResource()
{
    return false;
}

