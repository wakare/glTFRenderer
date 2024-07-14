#pragma once
#include <d3d12.h>
#include <memory>

#include "glTFRHI/RHIInterface/RHICommon.h"

class IRHITextureDescriptorAllocation;
class IRHIBufferDescriptorAllocation;
class IRHIRenderTarget;
class IRHITexture;
struct RHIConstantBufferViewDesc;
class IRHIBuffer;
struct RHIDescriptorHeapDesc;
class IRHIDevice;
class IRHIDescriptorAllocation;

class DX12DescriptorHeap
{
public:
    DX12DescriptorHeap();
    ~DX12DescriptorHeap();
    
    virtual bool InitDescriptorHeap(IRHIDevice& device, const RHIDescriptorHeapDesc& desc) ;
    unsigned GetUsedDescriptorCount() const;

    virtual bool CreateConstantBufferViewInDescriptorHeap(IRHIDevice& device, unsigned descriptor_offset, IRHIBuffer& buffer, const RHIConstantBufferViewDesc& desc, /*output*/
                                                          std::shared_ptr<IRHIDescriptorAllocation>& out_allocation) ;
    virtual bool CreateResourceDescriptorInHeap(IRHIDevice& device, const IRHIBuffer& buffer, const RHIDescriptorDesc& desc,
                                                          /*output*/
                                                          std::shared_ptr<IRHIBufferDescriptorAllocation>& out_allocation) ;
    virtual bool CreateResourceDescriptorInHeap(IRHIDevice& device, const IRHITexture& texture, const RHIDescriptorDesc& desc,
                                                          /*output*/
                                                          std::shared_ptr<IRHITextureDescriptorAllocation>& out_allocation) ;
    virtual bool CreateResourceDescriptorInHeap(IRHIDevice& device, const IRHIRenderTarget& render_target, const RHIDescriptorDesc& desc,
                                                          /*output*/
                                                          std::shared_ptr<IRHITextureDescriptorAllocation>& out_allocation) ;

    ID3D12DescriptorHeap* GetDescriptorHeap() {return m_descriptorHeap; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandleForHeapStart() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandleForHeapStart() const;

    D3D12_CPU_DESCRIPTOR_HANDLE GetAvailableCPUHandle() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetAvailableGPUHandle() const;
    
private:
    bool CreateSRVInHeap(IRHIDevice& device, unsigned descriptor_offset, ID3D12Resource* resource, const RHIDescriptorDesc& desc, /*output*/ RHIGPUDescriptorHandle& out_GPU_handle);
    bool CreateUAVInHeap(IRHIDevice& device, unsigned descriptor_offset, ID3D12Resource* resource, const RHIDescriptorDesc& desc, /*output*/ RHIGPUDescriptorHandle& out_GPU_handle);
    bool CreateRTVInHeap(IRHIDevice& device, unsigned descriptor_offset, ID3D12Resource* resource, const RHIDescriptorDesc& desc, /*output*/ RHICPUDescriptorHandle& out_CPU_handle);
    bool CreateDSVInHeap(IRHIDevice& device, unsigned descriptor_offset, ID3D12Resource* resource, const RHIDescriptorDesc& desc, /*output*/ RHICPUDescriptorHandle& out_CPU_handle);

    RHIDescriptorHeapDesc m_desc{};
    
    ID3D12DescriptorHeap* m_descriptorHeap;
    unsigned m_descriptor_increment_size;
    unsigned m_used_descriptor_count;

    std::map<ID3D12Resource*, std::vector<std::pair<RHIDescriptorDesc, RHIGPUDescriptorHandle>>> m_created_descriptors_info;
};
