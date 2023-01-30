#include "DX12DescriptorHeap.h"

#include "DX12Device.h"
#include "DX12Utils.h"

DX12DescriptorHeap::DX12DescriptorHeap()
    : m_descriptorHeap(nullptr)
{
}

DX12DescriptorHeap::~DX12DescriptorHeap()
{
    SAFE_RELEASE(m_descriptorHeap)
}

bool DX12DescriptorHeap::InitDescriptorHeap(IRHIDevice& device, const RHIDescriptorHeapDesc& desc)
{
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    
    // Init resource description heap
    D3D12_DESCRIPTOR_HEAP_DESC dxDesc = {};
    dxDesc.NumDescriptors = desc.maxDescriptorCount;
    dxDesc.Type = DX12ConverterUtils::ConvertToDescriptorHeapType(desc.type);
    dxDesc.Flags = desc.shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dxDesc.NodeMask = 0;
    
    THROW_IF_FAILED(dxDevice->CreateDescriptorHeap(&dxDesc, IID_PPV_ARGS(&m_descriptorHeap)))
    
    return true;
}
