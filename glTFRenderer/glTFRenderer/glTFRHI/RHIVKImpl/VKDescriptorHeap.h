#pragma once
#include "glTFRHI/RHIInterface/IRHIDescriptorHeap.h"

class VKDescriptorAllocation : public IRHIDescriptorAllocation
{
public:
    virtual bool InitFromBuffer(const IRHIBuffer& buffer) override;
};

class VKDescriptorTable : public IRHIDescriptorTable
{
public:
    virtual bool Build(IRHIDevice& device, const std::vector<std::shared_ptr<IRHIDescriptorAllocation>>& descriptor_allocations) override;
};

class VKDescriptorHeap : public IRHIDescriptorHeap
{
public:
    virtual bool InitDescriptorHeap(IRHIDevice& device, const RHIDescriptorHeapDesc& desc) override;
    virtual unsigned GetUsedDescriptorCount() const override;

    virtual bool CreateConstantBufferViewInDescriptorHeap(IRHIDevice& device, unsigned descriptorOffset, IRHIBuffer& buffer, const RHIConstantBufferViewDesc& desc, /*output*/
                                                          std::shared_ptr<IRHIDescriptorAllocation>& out_allocation) override;
    virtual bool CreateResourceDescriptorInHeap(IRHIDevice& device, const IRHIBuffer& buffer, const RHIShaderResourceViewDesc& desc,
                                                          /*output*/
                                                          std::shared_ptr<IRHIDescriptorAllocation>& out_allocation) override;
    virtual bool CreateResourceDescriptorInHeap(IRHIDevice& device, const IRHITexture& texture, const RHIShaderResourceViewDesc& desc,
                                                          /*output*/
                                                          std::shared_ptr<IRHIDescriptorAllocation>& out_allocation) override;
    virtual bool CreateResourceDescriptorInHeap(IRHIDevice& device, const IRHIRenderTarget& renderTarget, const RHIShaderResourceViewDesc& desc,
                                                          /*output*/
                                                          std::shared_ptr<IRHIDescriptorAllocation>& out_allocation) override;
};
