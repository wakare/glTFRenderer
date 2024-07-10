#pragma once
#include <memory>

#include "RHICommon.h"

class IRHICommandList;
class IRHIRenderTarget;
class IRHITexture;
struct RHIMemoryManagerDescriptorMaxCapacity;
class IRHIBuffer;
class IRHIDescriptorAllocation;
class IRHIDevice;
class IRHIDescriptorHeap;

// Handle descriptor initialization and binding
class IRHIDescriptorManager
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIDescriptorManager)
    
    virtual bool Init(IRHIDevice& device, const RHIMemoryManagerDescriptorMaxCapacity& max_descriptor_capacity) = 0;
    virtual bool CreateDescriptor(IRHIDevice& device, IRHIBuffer& buffer, const RHIDescriptorDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation) = 0;
    virtual bool CreateDescriptor(IRHIDevice& device, IRHITexture& texture, const RHIDescriptorDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation) = 0;
    virtual bool CreateDescriptor(IRHIDevice& device, IRHIRenderTarget& texture, const RHIDescriptorDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation) = 0;

    virtual bool BindDescriptors(IRHICommandList& command_list) = 0;
    virtual bool BindGUIDescriptors(IRHICommandList& command_list) = 0;
};
