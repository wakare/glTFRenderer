#pragma once
#include "RHICommon.h"

class IRHIRootSignature;
class IRHICommandList;
class IRHIDescriptorAllocation;
class IRHIDescriptorTable;
class IRHIDevice;

class RHICORE_API IRHIDescriptorUpdater
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIDescriptorUpdater)
    
    virtual bool BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline, const RootSignatureAllocation& root_signature_allocation, const IRHIDescriptorAllocation& descriptor) = 0;
    virtual bool BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline, const RootSignatureAllocation& root_signature_allocation, const IRHIDescriptorTable& descriptor_table, RHIDescriptorRangeType
                                            descriptor_type) = 0;

    virtual bool FinalizeUpdateDescriptors(IRHIDevice& device, IRHICommandList& command_list, IRHIRootSignature& root_signature) = 0;
};
