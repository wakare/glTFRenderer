#pragma once
#include "glTFRHI/RHIInterface/IRHIDescriptorUpdater.h"

class DX12DescriptorUpdater : public IRHIDescriptorUpdater
{
public:
    virtual bool BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline, const RootSignatureAllocation& root_signature_allocation, const IRHIDescriptorAllocation& allocation) override;
    virtual bool BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline, const RootSignatureAllocation& root_signature_allocation, const IRHIDescriptorTable& allocation_table) override;

    virtual bool FinalizeUpdateDescriptors(IRHIDevice& device, IRHICommandList& command_list, IRHIRootSignature& root_signature) override;
};
