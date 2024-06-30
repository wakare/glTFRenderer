#pragma once
#include "IRHIBuffer.h"
#include "IRHIDevice.h"
#include "IRHIMemoryAllocator.h"
#include "IRHIResource.h"
#include "IRHITexture.h"

class IRHIBufferAllocation : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIBufferAllocation)

    std::shared_ptr<IRHIBuffer> m_buffer {nullptr};
};

class IRHITextureAllocation : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHITextureAllocation)

    std::shared_ptr<IRHITexture> m_texture {nullptr};
};

// Hold descriptor heap which store cbv for gpu memory
class IRHIMemoryManager : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIMemoryManager)
    
    virtual bool InitMemoryManager(IRHIDevice& device, std::shared_ptr<IRHIMemoryAllocator> memory_allocator, unsigned max_descriptor_count) = 0;
    virtual bool AllocateBufferMemory(IRHIDevice& device, const RHIBufferDesc& buffer_desc, std::shared_ptr<IRHIBufferAllocation>& out_buffer_allocation) = 0;
    virtual bool UploadBufferData(IRHIBufferAllocation& buffer_allocation, const void* data, size_t offset, size_t size) = 0;
    virtual bool AllocateTextureMemoryAndUpload(IRHIDevice& device, IRHICommandList& command_list, const RHITextureDesc& buffer_desc, std::shared_ptr<IRHITextureAllocation>& out_buffer_allocation) = 0;
    
protected:
    std::shared_ptr<IRHIMemoryAllocator> m_allocator {nullptr};
};
