#include "DX12Buffer.h"

#include <cassert>

#include "d3dx12.h"
#include "DX12Utils.h"
#include <d3d12.h>

#include "DX12ConverterUtils.h"
#include "DX12Device.h"
#include "DX12MemoryManager.h"

bool DX12Buffer::Release(IRHIMemoryManager& memory_manager)
{
    if (m_mapped_gpu_buffer)
    {
        m_buffer->Unmap(0, &m_map_range);
        m_mapped_gpu_buffer = nullptr;
    }
    
    SAFE_RELEASE(m_buffer)
    return true;
}

bool DX12Buffer::CreateBuffer(IRHIDevice& device, const RHIBufferDesc& desc)
{
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();

    m_buffer_desc = desc;
    const bool contains_mipmap = desc.usage & RUF_CONTAINS_MIPMAP;
    unsigned mip_count = contains_mipmap ? static_cast<uint32_t>(std::floor(std::log2(std::max(desc.width, desc.height)))) + 1 : 1;
    
    const CD3DX12_HEAP_PROPERTIES heap_properties(DX12ConverterUtils::ConvertToHeapType(desc));
    CD3DX12_RESOURCE_DESC heap_resource_desc{};
    switch (desc.resource_type) {
        case RHIBufferResourceType::Buffer:
            heap_resource_desc = CD3DX12_RESOURCE_DESC::Buffer(desc.width);
            break;
        case RHIBufferResourceType::Tex1D:
            heap_resource_desc = CD3DX12_RESOURCE_DESC::Tex1D(DXGI_FORMAT_UNKNOWN, desc.width);
            break;
    case RHIBufferResourceType::Tex2D: 
            heap_resource_desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_UNKNOWN, desc.width, desc.height,  1, mip_count);
            break;
    case RHIBufferResourceType::Tex3D: 
            heap_resource_desc = CD3DX12_RESOURCE_DESC::Tex3D(DXGI_FORMAT_UNKNOWN, desc.width, desc.height, desc.depth ,mip_count);
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
        final_state = RHIResourceStateType::STATE_GENERIC_READ;
    }
    
    const D3D12_RESOURCE_STATES state = DX12ConverterUtils::ConvertToResourceState(final_state);
    THROW_IF_FAILED(dxDevice->CreateCommittedResource(
            &heap_properties, // this heap will be used to upload the constant buffer data
            D3D12_HEAP_FLAG_NONE, // no flags
            &heap_resource_desc, // size of the resource heap. Must be a multiple of 64KB for single-textures and constant buffers
            state, // will be data that is read from so we keep it in the generic read state
            nullptr, // we do not have use an optimized clear value for constant buffers
            IID_PPV_ARGS(&m_buffer)))
    m_buffer->SetName(desc.name.c_str());

    m_copy_req.row_byte_size.resize(mip_count);
    m_copy_req.row_pitch.resize(mip_count);
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints; footprints.resize(mip_count); 
    dxDevice->GetCopyableFootprints(&heap_resource_desc, 0, mip_count, 0, footprints.data(), nullptr, m_copy_req.row_byte_size.data(), &m_copy_req.total_size);
    for (uint32_t i = 0; i < footprints.size(); ++i)
    {
        m_copy_req.row_pitch[i] = footprints[i].Footprint.RowPitch;
    }
    
    return true;
}

bool DX12Buffer::UploadBufferFromCPU(const void* data, size_t dst_offset, size_t size)
{
    if (!m_mapped_gpu_buffer)
    {
        // Try mapping to cpu
        m_map_range = {0, 0};    // We do not intend to read from this resource on the CPU. (End is less than or equal to begin)
        THROW_IF_FAILED(m_buffer->Map(0, &m_map_range, reinterpret_cast<void**>(&m_mapped_gpu_buffer)))
    }
    
    assert((dst_offset + size) <= m_buffer_desc.width);
    
    memcpy(m_mapped_gpu_buffer + dst_offset, data, size);
    
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

RHIMipMapCopyRequirements DX12Buffer::GetMipMapRequirements() const
{
    return m_copy_req;
}

GPU_BUFFER_HANDLE_TYPE DX12Buffer::GetGPUBufferHandle() const
{
    return m_buffer->GetGPUVirtualAddress();
}
