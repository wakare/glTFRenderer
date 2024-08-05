#pragma once
#include "VKMemoryAllocator.h"
#include "glTFRHI/RHIInterface/IRHIMemoryManager.h"

class VKBufferAllocation : public IRHIBufferAllocation
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKBufferAllocation)

    VmaAllocation m_allocation;
    VmaAllocationInfo m_allocation_info;
};

class VKTextureAllocation : public IRHITextureAllocation
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKTextureAllocation)

    VmaAllocation m_allocation;
    VmaAllocationInfo m_allocation_info;
};

class VKMemoryManager : public IRHIMemoryManager
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKMemoryManager)
    
    virtual bool AllocateBufferMemory(IRHIDevice& device, const RHIBufferDesc& buffer_desc, std::shared_ptr<IRHIBufferAllocation>& out_buffer_allocation) override;
    virtual bool UploadBufferData(IRHIBufferAllocation& buffer_allocation, const void* data, size_t offset, size_t size) override;
    virtual bool AllocateTextureMemory(IRHIDevice& device, glTFRenderResourceManager& resource_manager, const RHITextureDesc& texture_desc, std::shared_ptr<IRHITextureAllocation>& out_texture_allocation) override;
    virtual bool AllocateTextureMemoryAndUpload(IRHIDevice& device, glTFRenderResourceManager& resource_manager, IRHICommandList& command_list, const RHITextureDesc& texture_desc, std::shared_ptr<IRHITextureAllocation>& out_texture_allocation) override;
    virtual bool CleanAllocatedResource() override;

protected:
    VmaAllocator GetVmaAllocator() const;

    std::vector<std::shared_ptr<IRHIBufferAllocation>> m_buffer_allocations;
    std::vector<std::shared_ptr<IRHITextureAllocation>> m_texture_allocations;
};
