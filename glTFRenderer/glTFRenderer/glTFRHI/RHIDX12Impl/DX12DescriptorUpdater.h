#pragma once
#include "glTFRHI/RHIInterface/IRHIDescriptorUpdater.h"

class DX12DescriptorUpdater : public IRHIDescriptorUpdater
{
public:
    virtual bool BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline, unsigned slot, const IRHIDescriptorAllocation& allocation) override;
    virtual bool BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline, unsigned slot, const IRHIDescriptorTable& allocation_table) override;

    virtual bool FinalizeUpdateDescriptors(IRHICommandList& command_list, IRHIRootSignature& root_signature) override;
};