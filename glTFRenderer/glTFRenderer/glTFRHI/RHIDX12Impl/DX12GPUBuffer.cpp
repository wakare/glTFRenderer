#include "DX12GPUBuffer.h"

#include <cassert>

#include "d3dx12.h"
#include "DX12Utils.h"
#include <d3d12.h>

#include "DX12Device.h"
#include "DX12GPUBufferManager.h"

DX12GPUBuffer::DX12GPUBuffer()
    : m_buffer(nullptr)
    , m_mappedGPUBuffer(nullptr)
{
}

bool DX12GPUBuffer::InitGPUBuffer(IRHIDevice& device, const RHIBufferDesc& desc)
{
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();

    m_bufferDesc = desc;
    assert(desc.size < (1024 * 64));
    CD3DX12_HEAP_PROPERTIES upload_heap_properties(DX12ConverterUtils::ConvertToHeapType(desc.type));
    CD3DX12_RESOURCE_DESC heap_resource_size = CD3DX12_RESOURCE_DESC::Buffer(1024 * 64);

    THROW_IF_FAILED(dxDevice->CreateCommittedResource(
            &upload_heap_properties, // this heap will be used to upload the constant buffer data
            D3D12_HEAP_FLAG_NONE, // no flags
            &heap_resource_size, // size of the resource heap. Must be a multiple of 64KB for single-textures and constant buffers
            D3D12_RESOURCE_STATE_GENERIC_READ, // will be data that is read from so we keep it in the generic read state
            nullptr, // we do not have use an optimized clear value for constant buffers
            IID_PPV_ARGS(&m_buffer)))
    m_buffer->SetName(desc.name.c_str());

    // mapping to cpu
    CD3DX12_RANGE readRange(0, 0);    // We do not intend to read from this resource on the CPU. (End is less than or equal to begin)
    THROW_IF_FAILED(m_buffer->Map(0, &readRange, reinterpret_cast<void**>(&m_mappedGPUBuffer)))
    
    return true;
}

bool DX12GPUBuffer::UploadBufferFromCPU(void* data, size_t size)
{
    assert(size == m_bufferDesc.size);
    
    memcpy(m_mappedGPUBuffer, data, size);
    return true;
}