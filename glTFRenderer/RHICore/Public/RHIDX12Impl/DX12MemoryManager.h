#pragma once
#include <memory>
#include "RHIInterface/IRHIMemoryManager.h"

class RHICORE_API DX12BufferAllocation : public IRHIBufferAllocation
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12BufferAllocation)
};

class RHICORE_API DX12TextureAllocation : public IRHITextureAllocation
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12TextureAllocation)
};

class RHICORE_API DX12MemoryManager : public IRHIMemoryManager
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12MemoryManager)
    
    virtual bool AllocateBufferMemory(IRHIDevice& device, const RHIBufferDesc& buffer_desc, std::shared_ptr<IRHIBufferAllocation>& out_buffer_allocation) override;
    
    virtual bool DownloadBufferData(IRHIBufferAllocation& buffer_allocation, void* data, size_t size) override;
    virtual bool AllocateTextureMemory(IRHIDevice& device, const RHITextureDesc& texture_desc, std::shared_ptr<IRHITextureAllocation>& out_texture_allocation) override;
    virtual bool ReleaseMemoryAllocation(IRHIMemoryAllocation& memory_allocation) override;
    
protected:
    virtual bool UploadBufferDataInner(IRHIBufferAllocation& buffer_allocation, const void* data, size_t dst_offset, size_t size) override;
};