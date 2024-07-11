#pragma once
#include <memory>

#include "IRHIResource.h"
#include "RHICommon.h"

class IRHICommandList;
class IRHIRenderTarget;
class IRHITexture;
struct RHIMemoryManagerDescriptorMaxCapacity;
class IRHIBuffer;
class IRHIDescriptorAllocation;
class IRHIDevice;

class IRHIDescriptorAllocation : public IRHIResource
{
public:
    virtual ~IRHIDescriptorAllocation() override = default;
    virtual bool InitFromBuffer(const IRHIBuffer& buffer) = 0;
    
    RHIDescriptorDesc m_view_desc;
};

class IRHIDescriptorTable : public IRHIResource
{
public:
    virtual bool Build(IRHIDevice& device, const std::vector<std::shared_ptr<IRHIDescriptorAllocation>>& descriptor_allocations) = 0;
};

// Handle descriptor initialization and binding
class IRHIDescriptorManager
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIDescriptorManager)
    
    virtual bool Init(IRHIDevice& device, const RHIMemoryManagerDescriptorMaxCapacity& max_descriptor_capacity) = 0;
    virtual bool CreateDescriptor(IRHIDevice& device, const IRHIBuffer& buffer, const RHIDescriptorDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation) = 0;
    virtual bool CreateDescriptor(IRHIDevice& device, const IRHITexture& texture, const RHIDescriptorDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation) = 0;
    virtual bool CreateDescriptor(IRHIDevice& device, const IRHIRenderTarget& texture, const RHIDescriptorDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation) = 0;

    virtual bool BindDescriptors(IRHICommandList& command_list) = 0;
    virtual bool BindGUIDescriptors(IRHICommandList& command_list) = 0;
};
