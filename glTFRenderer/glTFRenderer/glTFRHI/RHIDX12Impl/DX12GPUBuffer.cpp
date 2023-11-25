#include "DX12GPUBuffer.h"

#include <cassert>

#include "d3dx12.h"
#include "DX12Utils.h"
#include <d3d12.h>

#include "DX12Device.h"
#include "DX12GPUBufferManager.h"

DX12GPUBuffer::DX12GPUBuffer()
    : m_buffer(nullptr)
    , m_mapped_gpu_buffer(nullptr)
    , m_map_range()
{
}

DX12GPUBuffer::~DX12GPUBuffer()
{
    if (m_mapped_gpu_buffer)
    {
        m_buffer->Unmap(0, &m_map_range);
        m_mapped_gpu_buffer = nullptr;
    }
    
    SAFE_RELEASE(m_buffer)
}

bool DX12GPUBuffer::InitGPUBuffer(IRHIDevice& device, const RHIBufferDesc& desc)
{
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();

    m_buffer_desc = desc;
    const CD3DX12_HEAP_PROPERTIES heap_properties(DX12ConverterUtils::ConvertToHeapType(desc.type));
    CD3DX12_RESOURCE_DESC heap_resource_desc;
    switch (desc.resource_type) {
        case RHIBufferResourceType::Buffer:
            heap_resource_desc = CD3DX12_RESOURCE_DESC::Buffer(desc.width);
            break;
        case RHIBufferResourceType::Tex1D:
            heap_resource_desc = CD3DX12_RESOURCE_DESC::Tex1D(DX12ConverterUtils::ConvertToDXGIFormat(desc.resource_data_type), desc.width);
            break;
        case RHIBufferResourceType::Tex2D: 
            heap_resource_desc = CD3DX12_RESOURCE_DESC::Tex2D(DX12ConverterUtils::ConvertToDXGIFormat(desc.resource_data_type), desc.width, desc.height,  1, 1);
            break;
        case RHIBufferResourceType::Tex3D: 
            heap_resource_desc = CD3DX12_RESOURCE_DESC::Tex3D(DX12ConverterUtils::ConvertToDXGIFormat(desc.resource_data_type), desc.width, desc.height, desc.depth ,1);
            break;
    }
    
    if (desc.alignment)
    {
        heap_resource_desc.Alignment = desc.alignment;
    }

    if (desc.usage == RHIBufferUsage::ALLOW_UNORDER_ACCESS)
    {
        heap_resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    const D3D12_RESOURCE_STATES state = (desc.state == RHIResourceStateType::STATE_UNKNOWN) ? desc.type == RHIBufferType::Default ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_GENERIC_READ :
                                            DX12ConverterUtils::ConvertToResourceState(desc.state);
    
    THROW_IF_FAILED(dxDevice->CreateCommittedResource(
            &heap_properties, // this heap will be used to upload the constant buffer data
            D3D12_HEAP_FLAG_NONE, // no flags
            &heap_resource_desc, // size of the resource heap. Must be a multiple of 64KB for single-textures and constant buffers
            state, // will be data that is read from so we keep it in the generic read state
            nullptr, // we do not have use an optimized clear value for constant buffers
            IID_PPV_ARGS(&m_buffer)))
    m_buffer->SetName(desc.name.c_str());

    return true;
}

bool DX12GPUBuffer::UploadBufferFromCPU(const void* data, size_t dataOffset, size_t size)
{
    if (!m_mapped_gpu_buffer)
    {
        // Try mapping to cpu
        m_map_range = {0, 0};    // We do not intend to read from this resource on the CPU. (End is less than or equal to begin)
        THROW_IF_FAILED(m_buffer->Map(0, &m_map_range, reinterpret_cast<void**>(&m_mapped_gpu_buffer)))
    }
    
    assert((dataOffset + size) <= m_buffer_desc.width);
    
    memcpy(m_mapped_gpu_buffer + dataOffset, data, size);
    
    m_buffer->Unmap(0, nullptr);
    m_mapped_gpu_buffer = nullptr;
    
    return true;
}

GPU_BUFFER_HANDLE_TYPE DX12GPUBuffer::GetGPUBufferHandle()
{
    return m_buffer->GetGPUVirtualAddress();
}
