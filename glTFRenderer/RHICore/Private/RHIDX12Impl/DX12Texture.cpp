#include "DX12Texture.h"

#include <utility>
#include "DX12Buffer.h"
#include "DX12ConverterUtils.h"
#include "DX12Device.h"
#include "DX12Utils.h"
#include "RHIUtils.h"
#include "SceneFileLoader/glTFImageIOUtil.h"

ID3D12Resource* DX12Texture::GetRawResource()
{
    return m_buffer.Get();
}

const ID3D12Resource* DX12Texture::GetRawResource() const
{
    return m_buffer.Get();
}

bool DX12Texture::InitFromExternalResource(ID3D12Resource* raw_resource, const RHITextureDesc& desc)
{
    m_buffer = raw_resource;
    m_texture_desc = desc;
    return true;
}

bool DX12Texture::CreateTexture(IRHIDevice& device, const RHITextureDesc& desc)
{
    auto* dxDevice = dynamic_cast<DX12Device&>(device).GetDevice();

    m_texture_desc = desc;
    const bool contains_mipmap = desc.GetUsage() & RUF_CONTAINS_MIPMAP;
    unsigned mip_count = contains_mipmap ? static_cast<uint32_t>(std::floor(std::log2(std::max(desc.GetTextureWidth(), desc.GetTextureHeight())))) + 1 : 1;
    auto format = DX12ConverterUtils::ConvertToDXGIFormat(desc.GetDataFormat());
    
    const CD3DX12_HEAP_PROPERTIES heap_properties(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC heap_resource_desc = CD3DX12_RESOURCE_DESC::Tex2D(format, desc.GetTextureWidth(), desc.GetTextureHeight(),  1, mip_count);
    heap_resource_desc.Flags = DX12ConverterUtils::ConvertToResourceFlags(desc.GetUsage());
    
    RHIResourceStateType final_state = RHIResourceStateType::STATE_COMMON;
    bool need_clear = desc.GetUsage() & RUF_ALLOW_CLEAR;
    D3D12_CLEAR_VALUE clear_value = {};
    if (need_clear)
    {
        clear_value.Format = DX12ConverterUtils::ConvertToDXGIFormat(desc.GetClearValue().clear_format);
        if (IsDepthStencilFormat(desc.GetClearValue().clear_format))
        {
            clear_value.DepthStencil.Depth = desc.GetClearValue().clear_depth_stencil.clear_depth;
            clear_value.DepthStencil.Stencil = desc.GetClearValue().clear_depth_stencil.clear_stencil_value;
        }
        else
        {
            memcpy(clear_value.Color, desc.GetClearValue().clear_color, sizeof(clear_value.Color));
        }    
    }
    
    const D3D12_RESOURCE_STATES state = DX12ConverterUtils::ConvertToResourceState(final_state);
    THROW_IF_FAILED(dxDevice->CreateCommittedResource(
            &heap_properties, // this heap will be used to upload the constant buffer data
            D3D12_HEAP_FLAG_NONE, // no flags
            &heap_resource_desc, // size of the resource heap. Must be a multiple of 64KB for single-textures and constant buffers
            state, // will be data that is read from so we keep it in the generic read state
            need_clear ? &clear_value : nullptr, // we do not have use an optimized clear value for constant buffers
            IID_PPV_ARGS(&m_buffer)))
    m_buffer->SetName(to_wide_string(desc.GetName()).c_str());

    m_copy_requirements.row_byte_size.resize(mip_count);
    m_copy_requirements.row_pitch.resize(mip_count);
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints; footprints.resize(mip_count); 
    dxDevice->GetCopyableFootprints(&heap_resource_desc, 0, mip_count, 0, footprints.data(), nullptr, m_copy_requirements.row_byte_size.data(), &m_copy_requirements.total_size);
    for (uint32_t i = 0; i < footprints.size(); ++i)
    {
        m_copy_requirements.row_pitch[i] = footprints[i].Footprint.RowPitch;
    }
    
    return true;
}

bool DX12Texture::Release(IRHIMemoryManager& memory_manager)
{
    SAFE_RELEASE(m_buffer);
    return true;
}
