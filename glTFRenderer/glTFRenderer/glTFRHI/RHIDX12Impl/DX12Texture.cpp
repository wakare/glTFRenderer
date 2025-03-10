#include "DX12Texture.h"

#include <utility>
#include "DX12Buffer.h"
#include "DX12Device.h"
#include "DX12Utils.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIUtils.h"
#include "SceneFileLoader/glTFImageLoader.h"

ID3D12Resource* DX12Texture::GetRawResource()
{
    return m_raw_resource ? m_raw_resource.Get() :
        dynamic_cast<const DX12Buffer&>(*m_texture_buffer->m_buffer).GetRawBuffer();
}

const ID3D12Resource* DX12Texture::GetRawResource() const
{
    return m_raw_resource ? m_raw_resource.Get() :
        dynamic_cast<const DX12Buffer&>(*m_texture_buffer->m_buffer).GetRawBuffer();
}

std::shared_ptr<IRHIBufferAllocation> DX12Texture::GetTextureAllocation() const
{
    return m_texture_buffer;
}

bool DX12Texture::InitFromExternalResource(ID3D12Resource* raw_resource, const RHITextureDesc& desc)
{
    m_raw_resource = raw_resource;
    m_texture_desc = desc;
    return true;
}

bool DX12Texture::InitTexture(std::shared_ptr<IRHIBufferAllocation> buffer, const RHITextureDesc& desc)
{
    m_texture_desc.InitWithoutCopyData(desc);
    m_texture_buffer = std::move(buffer);
    return true;
}

bool DX12Texture::Release(glTFRenderResourceManager& gl_tf_render_resource_manager)
{
    SAFE_RELEASE(m_raw_resource);
    return true;
}
