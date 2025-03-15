#include "DX12Buffer.h"

#include <cassert>

#include "d3dx12.h"
#include "DX12Utils.h"
#include <d3d12.h>

#include "DX12ConverterUtils.h"
#include "DX12Device.h"
#include "DX12MemoryManager.h"

bool DX12Buffer::Release(glTFRenderResourceManager& gl_tf_render_resource_manager)
{
    if (m_mapped_gpu_buffer)
    {
        m_buffer->Unmap(0, &m_map_range);
        m_mapped_gpu_buffer = nullptr;
    }
    
    SAFE_RELEASE(m_buffer)
    return true;
}

bool DX12Buffer::InitGPUBuffer(IRHIDevice& device, const RHIBufferDesc& desc)
{
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();

    m_buffer_desc = desc;
    const bool contains_mipmap = desc.usage & RUF_CONTAINS_MIPMAP;
    const CD3DX12_HEAP_PROPERTIES heap_properties(DX12ConverterUtils::ConvertToHeapType(desc));
    CD3DX12_RESOURCE_DESC heap_resource_desc{};
    switch (desc.resource_type) {
        case RHIBufferResourceType::Buffer:
            heap_resource_desc = CD3DX12_RESOURCE_DESC::Buffer(desc.width);
            break;
        case RHIBufferResourceType::Tex1D:
            heap_resource_desc = CD3DX12_RESOURCE_DESC::Tex1D(DX12ConverterUtils::ConvertToDXGIFormat(desc.resource_data_type), desc.width);
            break;
    case RHIBufferResourceType::Tex2D: 
            heap_resource_desc = CD3DX12_RESOURCE_DESC::Tex2D(DX12ConverterUtils::ConvertToDXGIFormat(desc.resource_data_type), desc.width, desc.height,  1, contains_mipmap ? 0 : 1);
            break;
    case RHIBufferResourceType::Tex3D: 
            heap_resource_desc = CD3DX12_RESOURCE_DESC::Tex3D(DX12ConverterUtils::ConvertToDXGIFormat(desc.resource_data_type), desc.width, desc.height, desc.depth ,contains_mipmap ? 0 : 1);
            break;
    }
    
    if (desc.alignment)
    {
        heap_resource_desc.Alignment = desc.alignment;
    }

    heap_resource_desc.Flags = DX12ConverterUtils::ConvertToResourceFlags(desc.usage);
    
    RHIResourceStateType final_state = desc.state;
    if (final_state == RHIResourceStateType::STATE_UNDEFINED)
    {
        final_state = desc.type == RHIBufferType::Default ? RHIResourceStateType::STATE_COMMON : RHIResourceStateType::STATE_GENERIC_READ;
    }
    else if (desc.type == RHIBufferType::Upload)
    {
        final_state =  RHIResourceStateType::STATE_GENERIC_READ;
    }
    
    const D3D12_RESOURCE_STATES state = DX12ConverterUtils::ConvertToResourceState(final_state);
    const auto dx_clear_value = DX12ConverterUtils::ConvertToD3DClearValue(desc.clear_value);
    bool use_clear_value = desc.usage & (RUF_ALLOW_DEPTH_STENCIL | RUF_ALLOW_RENDER_TARGET);
    THROW_IF_FAILED(dxDevice->CreateCommittedResource(
            &heap_properties, // this heap will be used to upload the constant buffer data
            D3D12_HEAP_FLAG_NONE, // no flags
            &heap_resource_desc, // size of the resource heap. Must be a multiple of 64KB for single-textures and constant buffers
            state, // will be data that is read from so we keep it in the generic read state
            use_clear_value ? &dx_clear_value : nullptr, // we do not have use an optimized clear value for constant buffers
            IID_PPV_ARGS(&m_buffer)))
    m_buffer->SetName(desc.name.c_str());

    return true;
}

bool DX12Buffer::UploadBufferFromCPU(const void* data, size_t dataOffset, size_t size)
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

bool DX12Buffer::DownloadBufferToCPU(void* data, size_t size)
{
    if (!m_mapped_gpu_buffer)
    {
        D3D12_RANGE readRange = { 0, size };
        THROW_IF_FAILED(m_buffer->Map(0, &m_map_range, reinterpret_cast<void**>(&m_mapped_gpu_buffer)))
    }

    GLTF_CHECK(size <= m_buffer_desc.width);
    memcpy(data, m_mapped_gpu_buffer, size);

    m_buffer->Unmap(0, nullptr);
    m_mapped_gpu_buffer = nullptr;

    return true;
}

GPU_BUFFER_HANDLE_TYPE DX12Buffer::GetGPUBufferHandle() const
{
    return m_buffer->GetGPUVirtualAddress();
}
