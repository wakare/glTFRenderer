#pragma once
#include <memory>
#include "glTFRHI/RHIInterface/IRHIMemoryManager.h"

class DX12BufferAllocation : public IRHIBufferAllocation
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12BufferAllocation)
};

class DX12TextureAllocation : public IRHITextureAllocation
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12TextureAllocation)
};

class DX12MemoryManager : public IRHIMemoryManager
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12MemoryManager)
    
    virtual bool AllocateBufferMemory(IRHIDevice& device, const RHIBufferDesc& buffer_desc, std::shared_ptr<IRHIBufferAllocation>& out_buffer_allocation) override;
    virtual bool UploadBufferData(IRHIBufferAllocation& buffer_allocation, const void* data, size_t offset, size_t size) override;
    virtual bool AllocateTextureMemory(IRHIDevice& device, glTFRenderResourceManager& resource_manager, const RHITextureDesc& texture_desc, std::shared_ptr<IRHITextureAllocation>& out_buffer_allocation) override;
    virtual bool AllocateTextureMemoryAndUpload(IRHIDevice& device, glTFRenderResourceManager& resource_manager, IRHICommandList& command_list, const RHITextureDesc& texture_desc, std::shared_ptr<IRHITextureAllocation>& out_buffer_allocation) override;
    virtual bool CleanAllocatedResource() override;
};