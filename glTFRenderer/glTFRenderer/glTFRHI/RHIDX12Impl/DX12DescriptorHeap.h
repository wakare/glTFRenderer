#pragma once
#include <d3d12.h>

#include "../RHIInterface/IRHIDescriptorHeap.h"

class DX12DescriptorHeap : public IRHIDescriptorHeap
{
public:
    DX12DescriptorHeap();
    virtual ~DX12DescriptorHeap() override;
    
    virtual bool InitDescriptorHeap(IRHIDevice& device, const RHIDescriptorHeapDesc& desc) override;
    virtual RHIGPUDescriptorHandle GetGPUHandle(unsigned offsetInDescriptor) override;
    unsigned GetUsedDescriptorCount() const override;

    virtual bool CreateConstantBufferViewInDescriptorHeap(IRHIDevice& device, unsigned descriptor_offset, IRHIGPUBuffer& buffer, const RHIConstantBufferViewDesc& desc, /*output*/ RHIGPUDescriptorHandle& out_GPU_handle) override;
    virtual bool CreateShaderResourceViewInDescriptorHeap(IRHIDevice& device, unsigned descriptor_offset, IRHIGPUBuffer& buffer, const RHIShaderResourceViewDesc& desc, /*output*/ RHIGPUDescriptorHandle& out_GPU_handle) override;
    virtual bool CreateShaderResourceViewInDescriptorHeap(IRHIDevice& device, unsigned descriptor_offset, IRHIRenderTarget& render_target, const RHIShaderResourceViewDesc& desc, /*output*/ RHIGPUDescriptorHandle& out_GPU_handle) override;

    virtual bool CreateUnOrderAccessViewInDescriptorHeap(IRHIDevice& device, unsigned descriptor_offset, IRHIGPUBuffer& buffer, const RHIShaderResourceViewDesc& desc, /*output*/ RHIGPUDescriptorHandle& out_GPU_handle) override;
    virtual bool CreateUnOrderAccessViewInDescriptorHeap(IRHIDevice& device, unsigned descriptor_offset, IRHIRenderTarget& render_target, const RHIShaderResourceViewDesc& desc, /*output*/ RHIGPUDescriptorHandle& out_GPU_handle) override;
    
    ID3D12DescriptorHeap* GetDescriptorHeap() {return m_descriptorHeap; }
    
private:
    bool CreateSRVInHeap(IRHIDevice& device, unsigned descriptor_offset, ID3D12Resource* resource, const RHIShaderResourceViewDesc& desc, /*output*/ RHIGPUDescriptorHandle& out_GPU_handle);
    bool CreateUAVInHeap(IRHIDevice& device, unsigned descriptor_offset, ID3D12Resource* resource, const RHIShaderResourceViewDesc& desc, /*output*/ RHIGPUDescriptorHandle& out_GPU_handle);
    
    ID3D12DescriptorHeap* m_descriptorHeap;
    unsigned m_descriptorIncrementSize;
    unsigned m_used_descriptor_count;
};
