#pragma once
#include <d3d12.h>

#include "../RHIInterface/IRHIDescriptorHeap.h"

class DX12DescriptorHeap : public IRHIDescriptorHeap
{
public:
    DX12DescriptorHeap();
    virtual ~DX12DescriptorHeap() override;
    
    virtual bool InitDescriptorHeap(IRHIDevice& device, const RHIDescriptorHeapDesc& desc) override;
    unsigned GetUsedDescriptorCount() const override;

    virtual bool CreateConstantBufferViewInDescriptorHeap(IRHIDevice& device, unsigned descriptor_offset, IRHIBuffer& buffer, const RHIConstantBufferViewDesc& desc, /*output*/ RHIGPUDescriptorHandle& out_GPU_handle) override;
    virtual bool CreateShaderResourceViewInDescriptorHeap(IRHIDevice& device, IRHIBuffer& buffer, const RHIShaderResourceViewDesc& desc, /*output*/ RHIGPUDescriptorHandle& out_GPU_handle) override;
    virtual bool CreateShaderResourceViewInDescriptorHeap(IRHIDevice& device, IRHIRenderTarget& render_target, const RHIShaderResourceViewDesc& desc, /*output*/ RHIGPUDescriptorHandle& out_GPU_handle) override;

    ID3D12DescriptorHeap* GetDescriptorHeap() {return m_descriptorHeap; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandleForHeapStart() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandleForHeapStart() const;
    
private:
    bool CreateSRVInHeap(IRHIDevice& device, unsigned descriptor_offset, ID3D12Resource* resource, const RHIShaderResourceViewDesc& desc, /*output*/ RHIGPUDescriptorHandle& out_GPU_handle);
    bool CreateUAVInHeap(IRHIDevice& device, unsigned descriptor_offset, ID3D12Resource* resource, const RHIShaderResourceViewDesc& desc, /*output*/ RHIGPUDescriptorHandle& out_GPU_handle);
    
    ID3D12DescriptorHeap* m_descriptorHeap;
    unsigned m_descriptorIncrementSize;
    unsigned m_used_descriptor_count;

    std::map<ID3D12Resource*, std::vector<std::pair<RHIShaderResourceViewDesc, RHIGPUDescriptorHandle>>> m_created_descriptors_info;
};
