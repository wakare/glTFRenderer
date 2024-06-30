#include "VKDescriptorHeap.h"

bool VKDescriptorHeap::InitDescriptorHeap(IRHIDevice& device, const RHIDescriptorHeapDesc& desc)
{
    return true;
}

RHICPUDescriptorHandle VKDescriptorHeap::GetCPUHandle(unsigned offsetInDescriptor)
{
    return 0;
}

RHIGPUDescriptorHandle VKDescriptorHeap::GetGPUHandle(unsigned offsetInDescriptor)
{
    return 0;
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

bool VKDescriptorHeap::CreateUnOrderAccessViewInDescriptorHeap(IRHIDevice& device, unsigned descriptorOffset,
    IRHIBuffer& buffer, const RHIShaderResourceViewDesc& desc, RHIGPUDescriptorHandle& outGPUHandle)
{
    return true;
}

bool VKDescriptorHeap::CreateUnOrderAccessViewInDescriptorHeap(IRHIDevice& device,
                                                               IRHIRenderTarget& renderTarget, const RHIShaderResourceViewDesc& desc, RHIGPUDescriptorHandle& outGPUHandle)
{
    return true;
}
