#pragma once
#include "glTFRHI/RHIInterface/IRHIDescriptorManager.h"

class DX12DescriptorHeap;

class DX12DescriptorAllocation : public IRHIDescriptorAllocation
{
public:
    DX12DescriptorAllocation() = default;
    
    DX12DescriptorAllocation(RHIGPUDescriptorHandle gpu_handle, RHICPUDescriptorHandle cpu_handle)
        : m_gpu_handle(gpu_handle)
        , m_cpu_handle(cpu_handle)
    {}

    virtual bool InitFromBuffer(const IRHIBuffer& buffer) override;
    
    RHIGPUDescriptorHandle m_gpu_handle {UINT64_MAX};
    RHICPUDescriptorHandle m_cpu_handle {UINT64_MAX};
};

class DX12DescriptorTable : public IRHIDescriptorTable
{
public:
    virtual bool Build(IRHIDevice& device, const std::vector<std::shared_ptr<IRHIDescriptorAllocation>>& descriptor_allocations) override;

    RHIGPUDescriptorHandle m_gpu_handle {UINT64_MAX};
};

class DX12DescriptorManager : public IRHIDescriptorManager
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12DescriptorManager)
    
    virtual bool Init(IRHIDevice& device, const RHIMemoryManagerDescriptorMaxCapacity& max_descriptor_capacity) override;
    virtual bool CreateDescriptor(IRHIDevice& device, const IRHIBuffer& buffer, const RHIDescriptorDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation) override;
    virtual bool CreateDescriptor(IRHIDevice& device, const IRHITexture& texture, const RHIDescriptorDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation) override;
    virtual bool CreateDescriptor(IRHIDevice& device, const IRHIRenderTarget& texture, const RHIDescriptorDesc& desc, std::shared_ptr<IRHIDescriptorAllocation>& out_descriptor_allocation) override;

    virtual bool BindDescriptors(IRHICommandList& command_list) override;
    virtual bool BindGUIDescriptors(IRHICommandList& command_list) override;
    
    DX12DescriptorHeap& GetDescriptorHeap(RHIViewType type) const;
    DX12DescriptorHeap& GetDescriptorHeap(RHIDescriptorHeapType type) const;
    DX12DescriptorHeap& GetGUIDescriptorHeap() const;
    
protected:
    std::shared_ptr<DX12DescriptorHeap> m_CBV_SRV_UAV_heap {nullptr};
    std::shared_ptr<DX12DescriptorHeap> m_RTV_heap {nullptr};
    std::shared_ptr<DX12DescriptorHeap> m_DSV_heap {nullptr};
    std::shared_ptr<DX12DescriptorHeap> m_ImGUI_Heap {nullptr};
};
