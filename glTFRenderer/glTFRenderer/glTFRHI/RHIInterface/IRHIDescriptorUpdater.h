#pragma once
#include "glTFRenderPass/glTFRenderPassCommon.h"

class IRHIRootSignature;
class IRHICommandList;
class IRHIDescriptorAllocation;
class IRHIDescriptorTable;
class IRHIDevice;

class IRHIDescriptorUpdater
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIDescriptorUpdater)
    
    virtual bool BindTextureDescriptorTable(IRHICommandList& command_list, RHIPipelineType pipeline, const RootSignatureAllocation& root_signature_allocation, const IRHIDescriptorAllocation& allocation) = 0;
    virtual bool BindTextureDescriptorTable(IRHICommandList& command_list, RHIPipelineType pipeline, const RootSignatureAllocation& root_signature_allocation, const IRHIDescriptorTable& allocation_table, RHIDescriptorRangeType
                                            descriptor_type) = 0;

    virtual bool FinalizeUpdateDescriptors(IRHIDevice& device, IRHICommandList& command_list, IRHIRootSignature& root_signature) = 0;
};
