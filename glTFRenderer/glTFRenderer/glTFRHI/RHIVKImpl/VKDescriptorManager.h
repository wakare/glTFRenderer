#pragma once
#include "glTFRHI/RHIInterface/IRHIDescriptorManager.h"

class VKDescriptorAllocation : public IRHIDescriptorAllocation
{
public:
    virtual bool InitFromBuffer(const IRHIBuffer& buffer) override;
};

class VKDescriptorTable : public IRHIDescriptorTable
{
public:
    virtual bool Build(IRHIDevice& device, const std::vector<std::shared_ptr<IRHIDescriptorAllocation>>& descriptor_allocations) override;
};

class VKDescriptorManager : public IRHIDescriptorManager
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKDescriptorManager)

    virtual bool Init(IRHIDevice& device, const RHIMemoryManagerDescriptorMaxCapacity& max_descriptor_capacity);
    virtual bool CreateDescriptor(IRHIDevice& device, const IRHIBuffer& buffer, const RHIDescriptorDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation) override;
    virtual bool CreateDescriptor(IRHIDevice& device, const IRHITexture& texture, const RHIDescriptorDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation) override;
    virtual bool CreateDescriptor(IRHIDevice& device, const IRHIRenderTarget& texture, const RHIDescriptorDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation) override;

    virtual bool BindDescriptors(IRHICommandList& command_list) override;
    virtual bool BindGUIDescriptors(IRHICommandList& command_list) override;

};
