#pragma once
#include "RHIInterface/IRHIDescriptorUpdater.h"

class RHICORE_API DX12DescriptorUpdater : public IRHIDescriptorUpdater
{
public:
    virtual bool BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline, const RootSignatureAllocation& root_signature_allocation, const IRHIDescriptorAllocation& allocation) override;
    virtual bool BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline, const RootSignatureAllocation& root_signature_allocation, const IRHIDescriptorTable& allocation_table, RHIDescriptorRangeType
                                            descriptor_type) override;

    virtual bool FinalizeUpdateDescriptors(IRHIDevice& device, IRHICommandList& command_list, IRHIRootSignature& root_signature) override;
};
