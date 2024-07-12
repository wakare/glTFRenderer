#pragma once
#include "glTFRHI/RHIInterface/IRHIDescriptorUpdater.h"

class VKDescriptorUpdater : public IRHIDescriptorUpdater
{
public:
    virtual bool BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline, unsigned slot, const IRHIDescriptorAllocation& allocation) override;
    virtual bool BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline, unsigned slot, const IRHIDescriptorTable& allocation_table) override;
};
