#pragma once
#include <memory>
#include <optional>

#include "IRHIResource.h"
#include "RHICommon.h"

class IRHICommandList;
class IRHIRenderTarget;
class IRHITexture;
class IRHIBuffer;
class IRHIDevice;
struct DescriptorAllocationInfo;

class IRHIDescriptorAllocation : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIDescriptorAllocation)

    virtual bool Release(glTFRenderResourceManager&) override;
    
    virtual const RHIDescriptorDesc& GetDesc() const = 0;
};

class IRHIBufferDescriptorAllocation : public IRHIDescriptorAllocation
{
public:
    virtual bool InitFromBuffer(const std::shared_ptr<IRHIBuffer>& buffer, const RHIBufferDescriptorDesc& desc) = 0;
    virtual const RHIDescriptorDesc& GetDesc() const override;

    std::shared_ptr<IRHIBuffer> m_source;
    std::optional<RHIBufferDescriptorDesc> m_view_desc;
};

class IRHITextureDescriptorAllocation : public IRHIDescriptorAllocation
{
public:
    virtual const RHIDescriptorDesc& GetDesc() const override;

    std::shared_ptr<IRHITexture> m_source;
    std::optional<RHITextureDescriptorDesc> m_view_desc;
};

class IRHIDescriptorTable : public IRHIResource
{
public:
    virtual bool Build(IRHIDevice& device, const std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>>& descriptor_allocations) = 0;

    virtual bool Release(glTFRenderResourceManager&) override;
};

// Handle descriptor initialization and binding
class IRHIDescriptorManager : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIDescriptorManager)
    
    virtual bool Init(IRHIDevice& device, const DescriptorAllocationInfo& max_descriptor_capacity) = 0;
    virtual bool CreateDescriptor(IRHIDevice& device, const std::shared_ptr<IRHIBuffer>& buffer, const RHIBufferDescriptorDesc& desc, std::shared_ptr<IRHIBufferDescriptorAllocation>& out_descriptor_allocation) = 0;
    virtual bool CreateDescriptor(IRHIDevice& device, const std::shared_ptr<IRHITexture>& texture, const RHITextureDescriptorDesc& desc, std::shared_ptr<IRHITextureDescriptorAllocation>& out_descriptor_allocation) = 0;

    virtual bool BindDescriptorContext(IRHICommandList& command_list) = 0;
    virtual bool BindGUIDescriptorContext(IRHICommandList& command_list) = 0;
};
