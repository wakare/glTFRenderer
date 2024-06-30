#pragma once
#include "IRHIDevice.h"
#include "IRHIResource.h"

class IRHIRenderTarget;
class IRHIBuffer;

class IRHIDescriptorHeap: public IRHIResource
{
public:
    virtual bool InitDescriptorHeap(IRHIDevice& device, const RHIDescriptorHeapDesc& desc) = 0;
    virtual RHICPUDescriptorHandle GetCPUHandle(unsigned offsetInDescriptor) = 0;
    virtual RHIGPUDescriptorHandle GetGPUHandle(unsigned offsetInDescriptor) = 0;
    virtual unsigned GetUsedDescriptorCount() const = 0;

    virtual bool CreateConstantBufferViewInDescriptorHeap(IRHIDevice& device, unsigned descriptorOffset, IRHIBuffer& buffer, const RHIConstantBufferViewDesc& desc, /*output*/ RHIGPUDescriptorHandle& outGPUHandle) = 0;
    virtual bool CreateShaderResourceViewInDescriptorHeap(IRHIDevice& device, unsigned descriptorOffset, IRHIBuffer& buffer, const RHIShaderResourceViewDesc& desc, /*output*/ RHIGPUDescriptorHandle& outGPUHandle) = 0;
    virtual bool CreateShaderResourceViewInDescriptorHeap(IRHIDevice& device, unsigned descriptorOffset, IRHIRenderTarget& renderTarget, const RHIShaderResourceViewDesc& desc, /*output*/ RHIGPUDescriptorHandle& outGPUHandle) = 0;
    virtual bool CreateUnOrderAccessViewInDescriptorHeap(IRHIDevice& device, unsigned descriptorOffset, IRHIBuffer& buffer, const RHIShaderResourceViewDesc& desc, /*output*/ RHIGPUDescriptorHandle& outGPUHandle) = 0;
    virtual bool CreateUnOrderAccessViewInDescriptorHeap(IRHIDevice& device, unsigned descriptorOffset, IRHIRenderTarget& renderTarget, const RHIShaderResourceViewDesc& desc, /*output*/ RHIGPUDescriptorHandle& outGPUHandle) = 0;
};
