#include "VKDescriptorHeap.h"

bool VKDescriptorHeap::InitDescriptorHeap(IRHIDevice& device, const RHIDescriptorHeapDesc& desc)
{
    return true;
}

unsigned VKDescriptorHeap::GetUsedDescriptorCount() const
{
    return 0;
}

bool VKDescriptorHeap::CreateConstantBufferViewInDescriptorHeap(IRHIDevice& device, unsigned descriptorOffset,
    IRHIBuffer& buffer, const RHIConstantBufferViewDesc& desc, RHIGPUDescriptorHandle& outGPUHandle)
{
    return true;
}

bool VKDescriptorHeap::CreateShaderResourceViewInDescriptorHeap(IRHIDevice& device,
                                                                IRHIBuffer& buffer, const RHIShaderResourceViewDesc& desc, RHIGPUDescriptorHandle& outGPUHandle)
{
    return true;
}

bool VKDescriptorHeap::CreateShaderResourceViewInDescriptorHeap(IRHIDevice& device,
                                                                IRHIRenderTarget& renderTarget, const RHIShaderResourceViewDesc& desc, RHIGPUDescriptorHandle& outGPUHandle)
{
    return true;
}
