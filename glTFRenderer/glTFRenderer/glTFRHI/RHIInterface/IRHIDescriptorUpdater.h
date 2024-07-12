#pragma once
#include "glTFRenderPass/glTFRenderPassCommon.h"

class IRHICommandList;
class IRHIDescriptorAllocation;

class IRHIDescriptorUpdater
{
public:
    virtual bool BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline, unsigned slot, const IRHIDescriptorAllocation& allocation) = 0;
    virtual bool BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline, unsigned slot, const IRHIDescriptorTable& allocation_table) = 0;
};
