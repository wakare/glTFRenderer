#pragma once
#include "glTFRHI/RHIInterface/IRHIMemoryManager.h"

class VKBufferAllocation : public IRHIBufferAllocation
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKBufferAllocation)
};

class VKTextureAllocation : public IRHITextureAllocation
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKTextureAllocation)
};

class VKMemoryManager : public IRHIMemoryManager
{
public:
    virtual bool InitMemoryManager(IRHIDevice& device, std::shared_ptr<IRHIMemoryAllocator> memory_allocator, const RHIMemoryManagerDescriptorMaxCapacity&
                                   max_descriptor_capacity) override;
    virtual bool AllocateBufferMemory(IRHIDevice& device, const RHIBufferDesc& buffer_desc, std::shared_ptr<IRHIBufferAllocation>& out_buffer_allocation) override;
    virtual bool UploadBufferData(IRHIBufferAllocation& buffer_allocation, const void* data, size_t offset, size_t size) override;
    virtual bool AllocateTextureMemory(IRHIDevice& device, const RHITextureDesc& buffer_desc, std::shared_ptr<IRHITextureAllocation>& out_buffer_allocation) override;
    virtual bool AllocateTextureMemoryAndUpload(IRHIDevice& device, IRHICommandList& command_list, const RHITextureDesc& buffer_desc, std::shared_ptr<IRHITextureAllocation>& out_buffer_allocation) override;
    virtual bool CleanAllocatedResource() override;
};
