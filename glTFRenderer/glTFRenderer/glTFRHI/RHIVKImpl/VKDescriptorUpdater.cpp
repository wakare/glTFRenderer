#include "VKDescriptorUpdater.h"

bool VKDescriptorUpdater::BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline,
    unsigned slot, const IRHIDescriptorAllocation& allocation)
{
    return false;
}

bool VKDescriptorUpdater::BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline, unsigned slot,
    const IRHIDescriptorTable& allocation_table)
{
    return true;
}
