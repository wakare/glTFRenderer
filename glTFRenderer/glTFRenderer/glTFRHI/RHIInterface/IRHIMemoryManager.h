#pragma once
#include "IRHIBuffer.h"
#include "IRHIDescriptorManager.h"
#include "IRHIMemoryAllocator.h"
#include "IRHITexture.h"

class IRHIMemoryAllocation : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_VDTOR(IRHIMemoryAllocation)

    enum AllocationType
    {
        BUFFER,
        TEXTURE,
    };
    
    IRHIMemoryAllocation(AllocationType type)
        : m_allocation_type(type)
    {
        
    }

    AllocationType GetAllocationType() const
    {
        return m_allocation_type;
    }

    virtual bool Release(glTFRenderResourceManager&) override;
    
protected:
    const AllocationType m_allocation_type;
};

class IRHIBufferAllocation : public IRHIMemoryAllocation
{
public:
    DECLARE_NON_COPYABLE_AND_VDTOR(IRHIBufferAllocation)
    IRHIBufferAllocation() :
        IRHIMemoryAllocation(AllocationType::BUFFER)
    {
        
    }

    std::shared_ptr<IRHIBuffer> m_buffer {nullptr};
};

class IRHITextureAllocation : public IRHIMemoryAllocation
{
public:
    DECLARE_NON_COPYABLE_AND_VDTOR(IRHITextureAllocation)
    IRHITextureAllocation():
        IRHIMemoryAllocation(AllocationType::TEXTURE)
    {
        
    }

    std::shared_ptr<IRHITexture> m_texture {nullptr};
};

struct DescriptorAllocationInfo
{
    unsigned cbv_srv_uav_size;
    unsigned rtv_size;
    unsigned dsv_size;
};

struct TempBufferInfo
{
    enum
    {
        TEMP_BUFFER_FRAME_LIFE_TIME = 3,
    };
    
    RHIBufferDesc desc;
    std::shared_ptr<IRHIBufferAllocation> allocation;
    int remain_frame_to_reuse;
};

class RHITempBufferPool
{
public:
    bool TryGetBuffer(const RHIBufferDesc& buffer_desc, std::shared_ptr<IRHIBufferAllocation>& out_buffer);
    void AddBufferToPool(const TempBufferInfo& buffer_info);
    void TickFrame();
    void Clear();
    
protected:
    std::vector<TempBufferInfo> m_temp_upload_buffer_allocations;
};

// Hold descriptor heap which store cbv for gpu memory
class IRHIMemoryManager 
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIMemoryManager)
    
    virtual bool InitMemoryManager(IRHIDevice& device, const IRHIFactory& factory, const DescriptorAllocationInfo&
                                   descriptor_allocation_info);
    
    virtual bool AllocateBufferMemory(IRHIDevice& device, const RHIBufferDesc& buffer_desc, std::shared_ptr<IRHIBufferAllocation>& out_buffer_allocation) = 0;
    virtual bool UploadBufferData(IRHIBufferAllocation& buffer_allocation, const void* data, size_t offset, size_t size) = 0;
    virtual bool DownloadBufferData(IRHIBufferAllocation& buffer_allocation, void* data, size_t size) = 0;
    virtual bool AllocateTextureMemory(IRHIDevice& device, glTFRenderResourceManager& resource_manager, const RHITextureDesc& buffer_desc, std::shared_ptr<IRHITextureAllocation>& out_buffer_allocation) = 0;
    virtual bool ReleaseMemoryAllocation(glTFRenderResourceManager& resource_manager, IRHIMemoryAllocation& memory_allocation) = 0;
    virtual bool ReleaseAllResource(glTFRenderResourceManager& resource_manager);

    bool AllocateTextureMemoryAndUpload(IRHIDevice& device, glTFRenderResourceManager& resource_manager, IRHICommandList& command_list, const RHITextureDesc& buffer_desc, std::shared_ptr<IRHITextureAllocation>& out_buffer_allocation);
    
    IRHIDescriptorManager& GetDescriptorManager() const;
    void TickFrame();
    bool AllocateTempUploadBufferMemory(IRHIDevice& device, const RHIBufferDesc& buffer_desc, std::shared_ptr<IRHIBufferAllocation>& out_buffer_allocation);
    
protected:
    RHITempBufferPool m_temp_buffer_pool;
    
    std::vector<std::shared_ptr<IRHIBufferAllocation>> m_buffer_allocations;
    std::vector<std::shared_ptr<IRHITextureAllocation>> m_texture_allocations;
    
    std::shared_ptr<IRHIDescriptorManager> m_descriptor_manager;
};
