#include "VKDescriptorUpdater.h"

bool VKDescriptorUpdater::BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline,
    unsigned slot, const IRHIDescriptorAllocation& allocation)
{
    return false;
}

bool VKDescriptorUpdater::BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline, unsigned slot,
    const IRHIDescriptorTable& allocation_table)
{
    return false;
}

bool VKDescriptorUpdater::FinalizeUpdateDescriptors(IRHICommandList& command_list)
{
    return false;
}
