#pragma once
#include "DX12Common.h"
#include "IRHIResource.h"
#include "RHICommon.h"
#include <memory>

class IRHITextureDescriptorAllocation;
class IRHIBufferDescriptorAllocation;
class IRHIRenderTarget;
class IRHITexture;
struct RHIConstantBufferViewDesc;
class IRHIBuffer;
struct RHIDescriptorHeapDesc;
class IRHIDevice;
class IRHIDescriptorAllocation;

class DX12DescriptorHeap : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12DescriptorHeap)
    
    virtual bool InitDescriptorHeap(IRHIDevice& device, const RHIDescriptorHeapDesc& desc) ;
    unsigned GetUsedDescriptorCount() const;

    virtual bool CreateConstantBufferViewInDescriptorHeap(IRHIDevice& device, unsigned descriptor_offset, std::shared_ptr<IRHIBuffer> buffer, const RHIConstantBufferViewDesc& desc, /*output*/
                                                          std::shared_ptr<IRHIDescriptorAllocation>& out_allocation) ;
    virtual bool CreateResourceDescriptorInHeap(IRHIDevice& device, const std::shared_ptr<IRHIBuffer>& buffer, const RHIDescriptorDesc& desc,
                                                          /*output*/
                                                          std::shared_ptr<IRHIBufferDescriptorAllocation>& out_allocation) ;
    virtual bool CreateResourceDescriptorInHeap(IRHIDevice& device, const std::shared_ptr<IRHITexture>& texture, const RHIDescriptorDesc& desc,
                                                          /*output*/
                                                          std::shared_ptr<IRHITextureDescriptorAllocation>& out_allocation) ;

    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
    ID3D12DescriptorHeap* GetDescriptorHeap() {return m_descriptorHeap.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandleForHeapStart() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandleForHeapStart() const;

    D3D12_CPU_DESCRIPTOR_HANDLE GetAvailableCPUHandle() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetAvailableGPUHandle() const;
    
private:
    bool CreateSRVInHeap(IRHIDevice& device, unsigned descriptor_offset, ID3D12Resource* resource, const RHIDescriptorDesc& desc, RHICPUDescriptorHandle
                         & out_CPU_handle,/*output*/ RHIGPUDescriptorHandle& out_GPU_handle);
    bool CreateUAVInHeap(IRHIDevice& device, unsigned descriptor_offset, ID3D12Resource* resource, const RHIDescriptorDesc& desc, RHICPUDescriptorHandle
                         & out_CPU_handle,/*output*/ RHIGPUDescriptorHandle& out_GPU_handle);
    bool CreateRTVInHeap(IRHIDevice& device, unsigned descriptor_offset, ID3D12Resource* resource, const RHIDescriptorDesc& desc, /*output*/ RHICPUDescriptorHandle& out_CPU_handle);
    bool CreateDSVInHeap(IRHIDevice& device, unsigned descriptor_offset, ID3D12Resource* resource, const RHIDescriptorDesc& desc, /*output*/ RHICPUDescriptorHandle& out_CPU_handle);

    RHIDescriptorHeapDesc m_desc{};
    
    ComPtr<ID3D12DescriptorHeap> m_descriptorHeap {nullptr};
    unsigned m_descriptor_increment_size {0};
    unsigned m_used_descriptor_count {0};

    std::map<ID3D12Resource*, std::vector<std::pair<RHIDescriptorDesc, std::pair<RHICPUDescriptorHandle, RHIGPUDescriptorHandle>>>> m_created_descriptors_info;
};
