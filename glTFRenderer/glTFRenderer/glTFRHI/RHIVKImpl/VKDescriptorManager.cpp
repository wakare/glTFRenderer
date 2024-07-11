#include "VKDescriptorManager.h"

bool VKDescriptorAllocation::InitFromBuffer(const IRHIBuffer& buffer)
{
    return false;
}

bool VKDescriptorTable::Build(IRHIDevice& device,
    const std::vector<std::shared_ptr<IRHIDescriptorAllocation>>& descriptor_allocations)
{
    return false;
}

bool VKDescriptorManager::Init(IRHIDevice& device, const RHIMemoryManagerDescriptorMaxCapacity& max_descriptor_capacity)
{
    return false;
}

bool VKDescriptorManager::CreateDescriptor(IRHIDevice& device, const IRHIBuffer& buffer, const RHIDescriptorDesc& desc,
                                           std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation)
{
    return false;
}

bool VKDescriptorManager::CreateDescriptor(IRHIDevice& device, const IRHITexture& texture, const RHIDescriptorDesc& desc,
                                           std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation)
{
    return false;
}

bool VKDescriptorManager::CreateDescriptor(IRHIDevice& device, const IRHIRenderTarget& texture, const RHIDescriptorDesc& desc,
                                           std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation)
{
    return false;
}

bool VKDescriptorManager::BindDescriptors(IRHICommandList& command_list)
{
    return false;
}

bool VKDescriptorManager::BindGUIDescriptors(IRHICommandList& command_list)
{
    return false;
}
