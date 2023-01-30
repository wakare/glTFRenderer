#pragma once
#include <d3d12.h>

#include "../RHIInterface/IRHIDescriptorHeap.h"

class DX12DescriptorHeap : public IRHIDescriptorHeap
{
public:
    DX12DescriptorHeap();
    virtual ~DX12DescriptorHeap() override;
    
    virtual bool InitDescriptorHeap(IRHIDevice& device, const RHIDescriptorHeapDesc& desc) override;
    ID3D12DescriptorHeap* GetDescriptorHeap() {return m_descriptorHeap; }
    
private:
    ID3D12DescriptorHeap* m_descriptorHeap;
};
