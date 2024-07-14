#pragma once
#include <memory>
#include <optional>

#include "IRHIResource.h"
#include "RHICommon.h"

class IRHICommandList;
class IRHIRenderTarget;
class IRHITexture;
struct RHIMemoryManagerDescriptorMaxCapacity;
class IRHIBuffer;
class IRHIDevice;

class IRHIDescriptorAllocation : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIDescriptorAllocation)

    virtual const RHIDescriptorDesc& GetDesc() const = 0;
};

class IRHIBufferDescriptorAllocation : public IRHIDescriptorAllocation
{
public:
    virtual bool InitFromBuffer(const IRHIBuffer& buffer, const RHIBufferDescriptorDesc& desc) = 0;
    
    virtual const RHIDescriptorDesc& GetDesc() const override;
    
    std::optional<RHIBufferDescriptorDesc> m_view_desc;
};

class IRHITextureDescriptorAllocation : public IRHIDescriptorAllocation
{
public:
    virtual const RHIDescriptorDesc& GetDesc() const override;
    
    std::optional<RHITextureDescriptorDesc> m_view_desc;
};

class IRHIDescriptorTable : public IRHIResource
{
public:
    virtual bool Build(IRHIDevice& device, const std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>>& descriptor_allocations) = 0;
};

// Handle descriptor initialization and binding
class IRHIDescriptorManager
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIDescriptorManager)
    
    virtual bool Init(IRHIDevice& device, const RHIMemoryManagerDescriptorMaxCapacity& max_descriptor_capacity) = 0;
    virtual bool CreateDescriptor(IRHIDevice& device, const IRHIBuffer& buffer, const RHIBufferDescriptorDesc& desc, std::shared_ptr<IRHIBufferDescriptorAllocation>& out_descriptor_allocation) = 0;
    virtual bool CreateDescriptor(IRHIDevice& device, const IRHITexture& texture, const RHITextureDescriptorDesc& desc, std::shared_ptr<IRHITextureDescriptorAllocation>& out_descriptor_allocation) = 0;
    virtual bool CreateDescriptor(IRHIDevice& device, const IRHIRenderTarget& texture, const RHITextureDescriptorDesc& desc, std::shared_ptr<IRHITextureDescriptorAllocation>& out_descriptor_allocation) = 0;

    virtual bool BindDescriptors(IRHICommandList& command_list) = 0;
    virtual bool BindGUIDescriptors(IRHICommandList& command_list) = 0;
};
