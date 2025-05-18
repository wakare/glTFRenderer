#pragma once
#include <utility>

#include "IRHIDescriptorManager.h"

class DX12DescriptorHeap;

class DX12BufferDescriptorAllocation : public IRHIBufferDescriptorAllocation
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12BufferDescriptorAllocation)
    
    bool InitHandle(RHIGPUDescriptorHandle gpu_handle, RHICPUDescriptorHandle cpu_handle);
    
    virtual bool InitFromBuffer(const std::shared_ptr<IRHIBuffer>& buffer, const RHIBufferDescriptorDesc& desc) override;
    
    RHIGPUDescriptorHandle m_gpu_handle {UINT64_MAX};
    RHICPUDescriptorHandle m_cpu_handle {UINT64_MAX};
};

class DX12TextureDescriptorAllocation : public IRHITextureDescriptorAllocation
{
public:
    DX12TextureDescriptorAllocation() = default;
    
    DX12TextureDescriptorAllocation(RHIGPUDescriptorHandle gpu_handle, RHICPUDescriptorHandle cpu_handle, std::shared_ptr<IRHITexture> texture, const RHITextureDescriptorDesc& desc)
        : m_gpu_handle(gpu_handle)
        , m_cpu_handle(cpu_handle)
    {
        m_source = std::move(texture);
        m_view_desc = desc;
    }

    RHIGPUDescriptorHandle m_gpu_handle {UINT64_MAX};
    RHICPUDescriptorHandle m_cpu_handle {UINT64_MAX};
};

class DX12DescriptorTable : public IRHIDescriptorTable
{
public:
    virtual bool Build(IRHIDevice& device, const std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>>& descriptor_allocations) override;

    RHIGPUDescriptorHandle m_gpu_handle {UINT64_MAX};
};

class DX12DescriptorManager : public IRHIDescriptorManager
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12DescriptorManager)
    
    virtual bool Init(IRHIDevice& device, const DescriptorAllocationInfo& max_descriptor_capacity) override;
    virtual bool CreateDescriptor(IRHIDevice& device, const std::shared_ptr<IRHIBuffer>& buffer, const RHIBufferDescriptorDesc& desc, std::shared_ptr<IRHIBufferDescriptorAllocation>& out_descriptor_allocation) override;
    virtual bool CreateDescriptor(IRHIDevice& device, const std::shared_ptr<IRHITexture>& texture, const RHITextureDescriptorDesc& desc, std::shared_ptr<IRHITextureDescriptorAllocation>& out_descriptor_allocation) override;

    virtual bool BindDescriptorContext(IRHICommandList& command_list) override;
    virtual bool BindGUIDescriptorContext(IRHICommandList& command_list) override;
    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
    DX12DescriptorHeap& GetDescriptorHeap(RHIViewType type) const;
    DX12DescriptorHeap& GetDescriptorHeap(RHIDescriptorHeapType type) const;
    DX12DescriptorHeap& GetGUIDescriptorHeap() const;
    
protected:
    std::shared_ptr<DX12DescriptorHeap> m_CBV_SRV_UAV_gpu_heap {nullptr};
    std::shared_ptr<DX12DescriptorHeap> m_CBV_SRV_UAV_cpu_heap {nullptr};
    std::shared_ptr<DX12DescriptorHeap> m_RTV_heap {nullptr};
    std::shared_ptr<DX12DescriptorHeap> m_DSV_heap {nullptr};
    std::shared_ptr<DX12DescriptorHeap> m_ImGUI_Heap {nullptr};
};
