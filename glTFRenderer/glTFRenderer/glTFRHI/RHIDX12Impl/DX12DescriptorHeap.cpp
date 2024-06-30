#include "DX12DescriptorHeap.h"

#include "d3dx12.h"
#include "DX12ConverterUtils.h"
#include "DX12Device.h"
#include "DX12Buffer.h"
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

RHICPUDescriptorHandle DX12DescriptorHeap::GetCPUHandle(unsigned offsetInDescriptor)
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_handle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
    cpu_handle.Offset(offsetInDescriptor, m_descriptorIncrementSize);
    return cpu_handle.ptr;
}

RHIGPUDescriptorHandle DX12DescriptorHeap::GetGPUHandle(unsigned offsetInDescriptor)
{
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpu_handle(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
    gpu_handle.Offset(offsetInDescriptor, m_descriptorIncrementSize);
    return gpu_handle.ptr;
}

unsigned DX12DescriptorHeap::GetUsedDescriptorCount() const
{
    return m_used_descriptor_count;
}

bool DX12DescriptorHeap::CreateConstantBufferViewInDescriptorHeap(IRHIDevice& device, unsigned descriptor_offset,
    IRHIBuffer& buffer, const RHIConstantBufferViewDesc& desc, RHIGPUDescriptorHandle& out_GPU_handle)
{
    //TODO: Process offset for handle 
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    auto* dxBuffer = dynamic_cast<DX12Buffer&>(buffer).GetBuffer();
    
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

bool DX12DescriptorHeap::CreateShaderResourceViewInDescriptorHeap(IRHIDevice& device,
                                                                  IRHIBuffer& buffer, const RHIShaderResourceViewDesc& desc, RHIGPUDescriptorHandle& out_GPU_handle)
{
    auto* dxBuffer = dynamic_cast<DX12Buffer&>(buffer).GetBuffer();
    return CreateSRVInHeap(device, m_used_descriptor_count, dxBuffer, desc, out_GPU_handle);
}

bool DX12DescriptorHeap::CreateShaderResourceViewInDescriptorHeap(IRHIDevice& device,
                                                                  IRHIRenderTarget& render_target, const RHIShaderResourceViewDesc& desc, RHIGPUDescriptorHandle& out_GPU_handle)
{
    auto* dxRenderTarget = dynamic_cast<DX12RenderTarget*>(&render_target);
    return CreateSRVInHeap(device, m_used_descriptor_count, dxRenderTarget->GetResource(), desc, out_GPU_handle);
}

bool DX12DescriptorHeap::CreateUnOrderAccessViewInDescriptorHeap(IRHIDevice& device, unsigned descriptor_offset,
    IRHIBuffer& buffer, const RHIShaderResourceViewDesc& desc, RHIGPUDescriptorHandle& out_GPU_handle)
{
    auto* dxBuffer = dynamic_cast<DX12Buffer&>(buffer).GetBuffer();
    return CreateUAVInHeap(device, descriptor_offset, dxBuffer, desc, out_GPU_handle);
}

bool DX12DescriptorHeap::CreateUnOrderAccessViewInDescriptorHeap(IRHIDevice& device,
                                                                 IRHIRenderTarget& render_target, const RHIShaderResourceViewDesc& desc, RHIGPUDescriptorHandle& out_GPU_handle)
{
    auto* dxRenderTarget = dynamic_cast<DX12RenderTarget*>(&render_target);
    return CreateUAVInHeap(device, m_used_descriptor_count, dxRenderTarget->GetResource(), desc, out_GPU_handle);
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeap::GetCPUHandleForHeapStart() const
{
    return m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_GPU_DESCRIPTOR_HANDLE DX12DescriptorHeap::GetGPUHandleForHeapStart() const
{
    return m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();
}

bool DX12DescriptorHeap::CreateSRVInHeap(IRHIDevice& device, unsigned descriptor_offset,
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

bool DX12DescriptorHeap::CreateUAVInHeap(IRHIDevice& device, unsigned descriptor_offset, ID3D12Resource* resource,
    const RHIShaderResourceViewDesc& desc, RHIGPUDescriptorHandle& out_GPU_handle)
{
    //TODO: Process offset for handle 
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    
    D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
    UAVDesc.Format = DX12ConverterUtils::ConvertToDXGIFormat(desc.format);
    UAVDesc.ViewDimension = DX12ConverterUtils::ConvertToUAVDimensionType(desc.dimension);

    if (desc.dimension == RHIResourceDimension::TEXTURE2D)
    {
        UAVDesc.Texture2D.MipSlice = 0;
        UAVDesc.Texture2D.PlaneSlice = 0;    
    }
    else if (desc.dimension == RHIResourceDimension::BUFFER)
    {
        if (desc.use_count_buffer)
        {
            UAVDesc.Buffer.CounterOffsetInBytes = desc.count_buffer_offset;
        }
        UAVDesc.Buffer.StructureByteStride = desc.stride;
        UAVDesc.Buffer.FirstElement = 0;
        UAVDesc.Buffer.NumElements = desc.count;
        UAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
    const UINT descriptorIncrementSize = dxDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    cpuHandle.Offset(descriptor_offset, descriptorIncrementSize);
    
    dxDevice->CreateUnorderedAccessView(resource, desc.use_count_buffer ? resource : nullptr, &UAVDesc, cpuHandle);
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
    gpuHandle.Offset(descriptor_offset, descriptorIncrementSize);
    out_GPU_handle = gpuHandle.ptr;
    
    ++m_used_descriptor_count;
    
    return true;
}
