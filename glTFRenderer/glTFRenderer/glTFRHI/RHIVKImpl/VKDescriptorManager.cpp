#include "VKDescriptorManager.h"

bool VKDescriptorManager::Init(IRHIDevice& device, const RHIMemoryManagerDescriptorMaxCapacity& max_descriptor_capacity)
{
    return false;
}

bool VKDescriptorManager::CreateDescriptor(IRHIDevice& device, IRHIBuffer& buffer, const RHIDescriptorDesc& desc,
    std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation)
{
    return false;
}

bool VKDescriptorManager::CreateDescriptor(IRHIDevice& device, IRHITexture& texture, const RHIDescriptorDesc& desc,
    std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation)
{
    return false;
}

bool VKDescriptorManager::CreateDescriptor(IRHIDevice& device, IRHIRenderTarget& texture, const RHIDescriptorDesc& desc,
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
