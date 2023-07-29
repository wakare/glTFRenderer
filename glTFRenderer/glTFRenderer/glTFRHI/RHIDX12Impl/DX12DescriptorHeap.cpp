#include "DX12DescriptorHeap.h"

#include "d3dx12.h"
#include "DX12Device.h"
#include "DX12GPUBuffer.h"
#include "DX12RenderTarget.h"
#include "DX12Utils.h"

DX12DescriptorHeap::DX12DescriptorHeap()
    : m_descriptorHeap(nullptr)
    , m_descriptorIncrementSize(0)
    , m_used_descriptor_count(0)
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

    m_descriptorIncrementSize = dxDevice->GetDescriptorHandleIncrementSize(dxDesc.Type);
    
    return true;
}

RHIGPUDescriptorHandle DX12DescriptorHeap::GetGPUHandle(unsigned offsetInDescriptor)
{
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
    gpuHandle.Offset(offsetInDescriptor, m_descriptorIncrementSize);
    return gpuHandle.ptr;
}

unsigned DX12DescriptorHeap::GetUsedDescriptorCount() const
{
    return m_used_descriptor_count;
}

bool DX12DescriptorHeap::CreateConstantBufferViewInDescriptorHeap(IRHIDevice& device, unsigned descriptor_offset,
    IRHIGPUBuffer& buffer, const RHIConstantBufferViewDesc& desc, RHIGPUDescriptorHandle& out_GPU_handle)
{
    //TODO: Process offset for handle 
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    auto* dxBuffer = dynamic_cast<DX12GPUBuffer&>(buffer).GetBuffer();
    
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
    const UINT descriptorIncrementSize = dxDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    cpuHandle.Offset(descriptor_offset, descriptorIncrementSize);
    
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = dxBuffer->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = (desc.bufferSize + 255) & ~255;    // CB size is required to be 256-byte aligned.
    dxDevice->CreateConstantBufferView(&cbvDesc, cpuHandle);

    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
    gpuHandle.Offset(descriptor_offset, descriptorIncrementSize);
    out_GPU_handle = gpuHandle.ptr;

    ++m_used_descriptor_count;
    
    return true;
}

bool DX12DescriptorHeap::CreateShaderResourceViewInDescriptorHeap(IRHIDevice& device, unsigned descriptor_offset,
    IRHIGPUBuffer& buffer, const RHIShaderResourceViewDesc& desc, RHIGPUDescriptorHandle& out_GPU_handle)
{
    auto* dxBuffer = dynamic_cast<DX12GPUBuffer&>(buffer).GetBuffer();
    return CreateShaderResourceViewInDescriptorHeap(device, descriptor_offset, dxBuffer, desc, out_GPU_handle);
}

bool DX12DescriptorHeap::CreateShaderResourceViewInDescriptorHeap(IRHIDevice& device, unsigned descriptor_offset,
    IRHIRenderTarget& render_target, const RHIShaderResourceViewDesc& desc, RHIGPUDescriptorHandle& out_GPU_handle)
{
    auto* dxRenderTarget = dynamic_cast<DX12RenderTarget*>(&render_target);
    return CreateShaderResourceViewInDescriptorHeap(device, descriptor_offset, dxRenderTarget->GetResource(), desc, out_GPU_handle);
}

bool DX12DescriptorHeap::CreateShaderResourceViewInDescriptorHeap(IRHIDevice& device, unsigned descriptor_offset,
    ID3D12Resource* resource, const RHIShaderResourceViewDesc& desc, RHIGPUDescriptorHandle& out_GPU_handle)
{
    //TODO: Process offset for handle 
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DX12ConverterUtils::ConvertToDXGIFormat(desc.format);
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = DX12ConverterUtils::ConvertToSRVDimensionType(desc.dimension);

    // TODO: Config mip levels
    srvDesc.Texture2D.MipLevels = 1;

    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
    const UINT descriptorIncrementSize = dxDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    cpuHandle.Offset(descriptor_offset, descriptorIncrementSize);
    
    dxDevice->CreateShaderResourceView(resource, &srvDesc, cpuHandle);
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
    gpuHandle.Offset(descriptor_offset, descriptorIncrementSize);
    out_GPU_handle = gpuHandle.ptr;
    
    ++m_used_descriptor_count;
    
    return true;
}
