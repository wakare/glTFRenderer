#include "DX12GPUBufferManager.h"

#include <cassert>

#include "d3dx12.h"
#include "DX12Device.h"
#include "../RHIResourceFactoryImpl.hpp"

DX12GPUBufferManager::DX12GPUBufferManager()
    : m_CBVDescriptorHeap(nullptr)
{
}

DX12GPUBufferManager::~DX12GPUBufferManager()
{
    SAFE_RELEASE(m_CBVDescriptorHeap)
}

bool DX12GPUBufferManager::InitGPUBufferManager(IRHIDevice& device, size_t maxDescriptorCount)
{
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = maxDescriptorCount;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    THROW_IF_FAILED(dxDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_CBVDescriptorHeap)))

    return true;
}

bool DX12GPUBufferManager::CreateGPUBuffer(IRHIDevice& device, const RHIBufferDesc& bufferDesc, size_t& outBufferIndex)
{
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();
    
    std::shared_ptr<DX12GPUBuffer> gpuBuffer = RHIResourceFactory::CreateRHIResource<DX12GPUBuffer>();
    if (!gpuBuffer->InitGPUBuffer(device, bufferDesc))
    {
        assert(false);
        return false;
    }

    outBufferIndex = m_buffers.size();
    m_buffers.push_back(gpuBuffer);
    
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = gpuBuffer->GetBuffer()->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = (static_cast<unsigned>(bufferDesc.size) + 255) & ~255;    // CB size is required to be 256-byte aligned.
    dxDevice->CreateConstantBufferView(&cbvDesc, m_CBVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    
    return true;
}