#pragma once
#include "glTFRenderPass/glTFRenderPassCommon.h"

class IRHIRootSignature;
class IRHICommandList;
class IRHIDescriptorAllocation;

class IRHIDescriptorUpdater
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIDescriptorUpdater)
    
    virtual bool BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline, unsigned space, unsigned slot, const IRHIDescriptorAllocation& allocation) = 0;
    virtual bool BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline, unsigned space, unsigned slot, const IRHIDescriptorTable& allocation_table) = 0;

    virtual bool FinalizeUpdateDescriptors(IRHIDevice& device, IRHICommandList& command_list, IRHIRootSignature& root_signature) = 0;
};
