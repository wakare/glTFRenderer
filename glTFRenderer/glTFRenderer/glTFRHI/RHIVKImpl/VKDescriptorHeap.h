#pragma once
#include "glTFRHI/RHIInterface/IRHIDescriptorHeap.h"

class VKDescriptorHeap : public IRHIDescriptorHeap
{
public:
    virtual bool InitDescriptorHeap(IRHIDevice& device, const RHIDescriptorHeapDesc& desc) override;
    virtual unsigned GetUsedDescriptorCount() const override;

    virtual bool CreateConstantBufferViewInDescriptorHeap(IRHIDevice& device, unsigned descriptorOffset, IRHIBuffer& buffer, const RHIConstantBufferViewDesc& desc, /*output*/ RHIGPUDescriptorHandle& outGPUHandle) override;
    virtual bool CreateShaderResourceViewInDescriptorHeap(IRHIDevice& device, IRHIBuffer& buffer, const RHIShaderResourceViewDesc& desc, /*output*/ RHIGPUDescriptorHandle& outGPUHandle) override;
    virtual bool CreateShaderResourceViewInDescriptorHeap(IRHIDevice& device, IRHIRenderTarget& renderTarget, const RHIShaderResourceViewDesc& desc, /*output*/ RHIGPUDescriptorHandle& outGPUHandle) override;
};
