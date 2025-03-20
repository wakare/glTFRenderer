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
    virtual bool InitMemoryManager(IRHIDevice& device, const IRHIFactory& factory, const DescriptorAllocationInfo& max_descriptor_capacity) override;
    
    virtual bool AllocateBufferMemory(IRHIDevice& device, const RHIBufferDesc& buffer_desc, std::shared_ptr<IRHIBufferAllocation>& out_buffer_allocation) override;
    virtual bool UploadBufferData(IRHIBufferAllocation& buffer_allocation, const void* data, size_t offset, size_t size) override;
    virtual bool DownloadBufferData(IRHIBufferAllocation& buffer_allocation, void* data, size_t size) override;
    virtual bool AllocateTextureMemory(IRHIDevice& device, glTFRenderResourceManager& resource_manager, const RHITextureDesc& texture_desc, std::shared_ptr<IRHITextureAllocation>& out_texture_allocation) override;
    
    virtual bool ReleaseMemoryAllocation(glTFRenderResourceManager& resource_manager, IRHIMemoryAllocation& memory_allocation) override;
    virtual bool ReleaseAllResource(glTFRenderResourceManager& resource_manager) override;

protected:
    VmaAllocator GetVmaAllocator() const;

    std::shared_ptr<IRHIMemoryAllocator> m_allocator;
};
