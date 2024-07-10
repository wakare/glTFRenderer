#include "VKDescriptorHeap.h"

bool VKDescriptorAllocation::InitFromBuffer(const IRHIBuffer& buffer)
{
    return false;
}

bool VKDescriptorTable::Build(IRHIDevice& device,
    const std::vector<std::shared_ptr<IRHIDescriptorAllocation>>& descriptor_allocations)
{
    return false;
}

bool VKDescriptorHeap::InitDescriptorHeap(IRHIDevice& device, const RHIDescriptorHeapDesc& desc)
{
    return true;
}

unsigned VKDescriptorHeap::GetUsedDescriptorCount() const
{
    return 0;
}

bool VKDescriptorHeap::CreateConstantBufferViewInDescriptorHeap(IRHIDevice& device, unsigned descriptorOffset,
                                                                IRHIBuffer& buffer, const RHIConstantBufferViewDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_allocation)
{
    return false;
}

bool VKDescriptorHeap::CreateResourceDescriptorInHeap(IRHIDevice& device,
                                                                const IRHIBuffer& buffer, const RHIDescriptorDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_allocation)
{
    return false;
}

bool VKDescriptorHeap::CreateResourceDescriptorInHeap(IRHIDevice& device, const IRHITexture& texture,
    const RHIDescriptorDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_allocation)
{
    return false;
}

bool VKDescriptorHeap::CreateResourceDescriptorInHeap(IRHIDevice& device,
                                                                const IRHIRenderTarget& renderTarget, const RHIDescriptorDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>&
                                                                out_allocation)
{
    return true;
}
