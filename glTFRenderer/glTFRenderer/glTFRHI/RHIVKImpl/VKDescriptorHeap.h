#pragma once
#include "glTFRHI/RHIInterface/IRHIDescriptorHeap.h"

class VKDescriptorHeap : public IRHIDescriptorHeap
{
public:
    virtual bool InitDescriptorHeap(IRHIDevice& device, const RHIDescriptorHeapDesc& desc) override;
    virtual RHICPUDescriptorHandle GetCPUHandle(unsigned offsetInDescriptor) override;
    virtual RHIGPUDescriptorHandle GetGPUHandle(unsigned offsetInDescriptor) override;
    virtual unsigned GetUsedDescriptorCount() const override;

    virtual bool CreateConstantBufferViewInDescriptorHeap(IRHIDevice& device, unsigned descriptorOffset, IRHIGPUBuffer& buffer, const RHIConstantBufferViewDesc& desc, /*output*/ RHIGPUDescriptorHandle& outGPUHandle) override;
    virtual bool CreateShaderResourceViewInDescriptorHeap(IRHIDevice& device, unsigned descriptorOffset, IRHIGPUBuffer& buffer, const RHIShaderResourceViewDesc& desc, /*output*/ RHIGPUDescriptorHandle& outGPUHandle) override;
    virtual bool CreateShaderResourceViewInDescriptorHeap(IRHIDevice& device, unsigned descriptorOffset, IRHIRenderTarget& renderTarget, const RHIShaderResourceViewDesc& desc, /*output*/ RHIGPUDescriptorHandle& outGPUHandle) override;
    virtual bool CreateUnOrderAccessViewInDescriptorHeap(IRHIDevice& device, unsigned descriptorOffset, IRHIGPUBuffer& buffer, const RHIShaderResourceViewDesc& desc, /*output*/ RHIGPUDescriptorHandle& outGPUHandle) override;
    virtual bool CreateUnOrderAccessViewInDescriptorHeap(IRHIDevice& device, unsigned descriptorOffset, IRHIRenderTarget& renderTarget, const RHIShaderResourceViewDesc& desc, /*output*/ RHIGPUDescriptorHandle& outGPUHandle) override;
};
