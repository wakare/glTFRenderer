#pragma once
#include "IRHIDevice.h"
#include "IRHIResource.h"
#include "IRHITexture.h"

class IRHIRenderTarget;
class IRHIBuffer;

class IRHIDescriptorAllocation : public IRHIResource
{
public:
    virtual ~IRHIDescriptorAllocation() override = default;
    virtual bool InitFromBuffer(const IRHIBuffer& buffer) = 0;
    
    RHIDescriptorDesc m_view_desc;
};

class IRHIDescriptorTable : public IRHIResource
{
public:
    virtual bool Build(IRHIDevice& device, const std::vector<std::shared_ptr<IRHIDescriptorAllocation>>& descriptor_allocations) = 0;
};

class IRHIDescriptorHeap: public IRHIResource
{
public:
    virtual bool InitDescriptorHeap(IRHIDevice& device, const RHIDescriptorHeapDesc& desc) = 0;
    virtual unsigned GetUsedDescriptorCount() const = 0;

    virtual bool CreateConstantBufferViewInDescriptorHeap(IRHIDevice& device, unsigned descriptorOffset, IRHIBuffer& buffer, const RHIConstantBufferViewDesc& desc, /*output*/
                                                          std::shared_ptr<IRHIDescriptorAllocation>& out_allocation) = 0;
    virtual bool CreateResourceDescriptorInHeap(IRHIDevice& device, const IRHIBuffer& buffer, const RHIDescriptorDesc& desc,
                                                          /*output*/
                                                          std::shared_ptr<IRHIDescriptorAllocation>& out_allocation) = 0;
    virtual bool CreateResourceDescriptorInHeap(IRHIDevice& device, const IRHITexture& texture, const RHIDescriptorDesc& desc,
                                                          /*output*/
                                                          std::shared_ptr<IRHIDescriptorAllocation>& out_allocation) = 0;
    virtual bool CreateResourceDescriptorInHeap(IRHIDevice& device, const IRHIRenderTarget& render_target, const RHIDescriptorDesc& desc,
                                                          /*output*/
                                                          std::shared_ptr<IRHIDescriptorAllocation>& out_allocation) = 0;
    
protected:
    RHIDescriptorHeapDesc m_desc{};
};
