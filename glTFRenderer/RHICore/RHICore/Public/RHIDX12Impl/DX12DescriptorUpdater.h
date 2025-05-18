#pragma once
#include "IRHIDescriptorUpdater.h"

class DX12DescriptorUpdater : public IRHIDescriptorUpdater
{
public:
    virtual bool BindTextureDescriptorTable(IRHICommandList& command_list, RHIPipelineType pipeline, const RootSignatureAllocation& root_signature_allocation, const IRHIDescriptorAllocation& allocation) override;
    virtual bool BindTextureDescriptorTable(IRHICommandList& command_list, RHIPipelineType pipeline, const RootSignatureAllocation& root_signature_allocation, const IRHIDescriptorTable& allocation_table, RHIDescriptorRangeType
                                            descriptor_type) override;

    virtual bool FinalizeUpdateDescriptors(IRHIDevice& device, IRHICommandList& command_list, IRHIRootSignature& root_signature) override;
};
