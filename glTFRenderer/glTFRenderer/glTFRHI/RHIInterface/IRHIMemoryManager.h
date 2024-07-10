#pragma once
#include "IRHIBuffer.h"
#include "IRHIDescriptorHeap.h"
#include "IRHIDescriptorManager.h"
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

struct RHIMemoryManagerDescriptorMaxCapacity
{
    unsigned cbv_srv_uav_size;
    unsigned rtv_size;
    unsigned dsv_size;
};

// Hold descriptor heap which store cbv for gpu memory
class IRHIMemoryManager : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIMemoryManager)
    
    bool InitMemoryManager(IRHIDevice& device, const std::shared_ptr<IRHIMemoryAllocator>& memory_allocator, const RHIMemoryManagerDescriptorMaxCapacity&
                                   max_descriptor_capacity);
    
    virtual bool AllocateBufferMemory(IRHIDevice& device, const RHIBufferDesc& buffer_desc, std::shared_ptr<IRHIBufferAllocation>& out_buffer_allocation) = 0;
    virtual bool UploadBufferData(IRHIBufferAllocation& buffer_allocation, const void* data, size_t offset, size_t size) = 0;
    virtual bool AllocateTextureMemory(IRHIDevice& device, const RHITextureDesc& buffer_desc, std::shared_ptr<IRHITextureAllocation>& out_buffer_allocation) = 0;
    virtual bool AllocateTextureMemoryAndUpload(IRHIDevice& device, IRHICommandList& command_list, const RHITextureDesc& buffer_desc, std::shared_ptr<IRHITextureAllocation>& out_buffer_allocation) = 0;
    virtual bool CleanAllocatedResource() = 0;

    IRHIDescriptorManager& GetDescriptorManager() const;
    
protected:
    std::vector<std::shared_ptr<IRHIBuffer>> m_buffers;
    std::vector<std::shared_ptr<IRHITexture>> m_textures;
    
    std::shared_ptr<IRHIDescriptorManager> m_descriptor_manager;
    std::shared_ptr<IRHIMemoryAllocator> m_allocator;
};
