#pragma once
#include "DX12Common.h"
#include <memory>
#include <vector>

#include "DX12Buffer.h"
#include "glTFRHI/RHIInterface/IRHIDescriptorHeap.h"
#include "glTFRHI/RHIInterface/IRHIMemoryManager.h"

class DX12BufferAllocation : public IRHIBufferAllocation
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12BufferAllocation)
};

class DX12MemoryManager : public IRHIMemoryManager
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR(DX12MemoryManager)
    virtual ~DX12MemoryManager() override;
    
    virtual bool InitMemoryManager(IRHIDevice& device, std::shared_ptr<IRHIMemoryAllocator> memory_allocator, const RHIMemoryManagerDescriptorMaxCapacity&
                                   max_descriptor_capacity) override;
    virtual bool AllocateBufferMemory(IRHIDevice& device, const RHIBufferDesc& buffer_desc, std::shared_ptr<IRHIBufferAllocation>& out_buffer_allocation) override;
    virtual bool UploadBufferData(IRHIBufferAllocation& buffer_allocation, const void* data, size_t offset, size_t size) override;
    virtual bool AllocateTextureMemory(IRHIDevice& device, const RHITextureDesc& texture_desc, std::shared_ptr<IRHITextureAllocation>& out_buffer_allocation) override;
    virtual bool AllocateTextureMemoryAndUpload(IRHIDevice& device, IRHICommandList& command_list, const RHITextureDesc& texture_desc, std::shared_ptr<IRHITextureAllocation>& out_buffer_allocation) override;
    
    //ID3D12DescriptorHeap* GetDescriptorHeap() {return m_CBV_SRV_UAV_Heap.Get(); }
    virtual bool CleanAllocatedResource() override;
};