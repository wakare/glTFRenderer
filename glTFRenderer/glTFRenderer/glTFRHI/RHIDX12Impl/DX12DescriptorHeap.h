#pragma once
#include <d3d12.h>

#include "glTFRHI/RHIInterface/IRHIDescriptorHeap.h"

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

class DX12DescriptorHeap : public IRHIDescriptorHeap
{
public:
    DX12DescriptorHeap();
    virtual ~DX12DescriptorHeap() override;
    
    virtual bool InitDescriptorHeap(IRHIDevice& device, const RHIDescriptorHeapDesc& desc) override;
    unsigned GetUsedDescriptorCount() const override;

    virtual bool CreateConstantBufferViewInDescriptorHeap(IRHIDevice& device, unsigned descriptor_offset, IRHIBuffer& buffer, const RHIConstantBufferViewDesc& desc, /*output*/
                                                          std::shared_ptr<IRHIDescriptorAllocation>& out_allocation) override;
    virtual bool CreateResourceDescriptorInHeap(IRHIDevice& device, const IRHIBuffer& buffer, const RHIDescriptorDesc& desc,
                                                          /*output*/
                                                          std::shared_ptr<IRHIDescriptorAllocation>& out_allocation) override;
    virtual bool CreateResourceDescriptorInHeap(IRHIDevice& device, const IRHITexture& texture, const RHIDescriptorDesc& desc,
                                                          /*output*/
                                                          std::shared_ptr<IRHIDescriptorAllocation>& out_allocation) override;
    virtual bool CreateResourceDescriptorInHeap(IRHIDevice& device, const IRHIRenderTarget& render_target, const RHIDescriptorDesc& desc,
                                                          /*output*/
                                                          std::shared_ptr<IRHIDescriptorAllocation>& out_allocation) override;

    ID3D12DescriptorHeap* GetDescriptorHeap() {return m_descriptorHeap; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandleForHeapStart() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandleForHeapStart() const;
    
private:
    bool CreateSRVInHeap(IRHIDevice& device, unsigned descriptor_offset, ID3D12Resource* resource, const RHIDescriptorDesc& desc, /*output*/ RHIGPUDescriptorHandle& out_GPU_handle);
    bool CreateUAVInHeap(IRHIDevice& device, unsigned descriptor_offset, ID3D12Resource* resource, const RHIDescriptorDesc& desc, /*output*/ RHIGPUDescriptorHandle& out_GPU_handle);
    bool CreateRTVInHeap(IRHIDevice& device, unsigned descriptor_offset, ID3D12Resource* resource, const RHIDescriptorDesc& desc, /*output*/ RHICPUDescriptorHandle& out_CPU_handle);
    bool CreateDSVInHeap(IRHIDevice& device, unsigned descriptor_offset, ID3D12Resource* resource, const RHIDescriptorDesc& desc, /*output*/ RHICPUDescriptorHandle& out_CPU_handle);

    ID3D12DescriptorHeap* m_descriptorHeap;
    unsigned m_descriptor_increment_size;
    unsigned m_used_descriptor_count;

    std::map<ID3D12Resource*, std::vector<std::pair<RHIDescriptorDesc, RHIGPUDescriptorHandle>>> m_created_descriptors_info;
};
