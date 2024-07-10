#pragma once
#include "glTFRHI/RHIInterface/IRHIDescriptorManager.h"

class DX12DescriptorManager : public IRHIDescriptorManager
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12DescriptorManager)
    
    virtual bool Init(IRHIDevice& device, const RHIMemoryManagerDescriptorMaxCapacity& max_descriptor_capacity) override;
    virtual bool CreateDescriptor(IRHIDevice& device, IRHIBuffer& buffer, const RHIDescriptorDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation) override;
    virtual bool CreateDescriptor(IRHIDevice& device, IRHITexture& texture, const RHIDescriptorDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation) override;
    virtual bool CreateDescriptor(IRHIDevice& device, IRHIRenderTarget& texture, const RHIDescriptorDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation) override;

    virtual bool BindDescriptors(IRHICommandList& command_list) override;
    
    IRHIDescriptorHeap& GetDescriptorHeap(RHIViewType type) const;
    IRHIDescriptorHeap& GetDescriptorHeap(RHIDescriptorHeapType type) const;
    
protected:
    std::shared_ptr<IRHIDescriptorHeap> m_CBV_SRV_UAV_heap {nullptr};
    std::shared_ptr<IRHIDescriptorHeap> m_RTV_heap {nullptr};
    std::shared_ptr<IRHIDescriptorHeap> m_DSV_heap {nullptr};
};
