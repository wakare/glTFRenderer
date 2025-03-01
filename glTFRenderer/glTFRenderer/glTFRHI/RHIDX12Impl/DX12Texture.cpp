#include "DX12Texture.h"
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

bool DX12Texture::InitFromExternalResource(ID3D12Resource* raw_resource, const RHITextureDesc& desc)
{
    m_raw_resource = raw_resource;
    m_texture_desc = desc;
    return true;
}

bool DX12Texture::InitTexture(IRHIDevice& device, glTFRenderResourceManager& resource_manager, const RHITextureDesc& desc)
{
    GLTF_CHECK(!desc.HasTextureData());
    
    m_texture_desc.InitWithoutCopyData(desc);
    const RHIBufferDesc textureBufferDesc =
        {
        to_wide_string(desc.GetName()),
        m_texture_desc.GetTextureWidth(),
        m_texture_desc.GetTextureHeight(),
        1,
        RHIBufferType::Default,
        m_texture_desc.GetDataFormat(),
        RHIBufferResourceType::Tex2D,
        RHIResourceStateType::STATE_COMMON,
        desc.GetUsage(),
        0,
        desc.GetClearValue()
    };
    
    RETURN_IF_FALSE(resource_manager.GetMemoryManager().AllocateBufferMemory(device, textureBufferDesc, m_texture_buffer));
    
    return true;
}

bool DX12Texture::InitTextureAndUpload(IRHIDevice& device, glTFRenderResourceManager& resource_manager, IRHICommandList& command_list, const RHITextureDesc& desc)
{
    GLTF_CHECK(desc.HasTextureData());
    void* texture_data = desc.GetTextureData();
    
    m_texture_desc.InitWithoutCopyData(desc);
    
    const RHIBufferDesc textureBufferDesc =
        {
            to_wide_string(desc.GetName()),
            m_texture_desc.GetTextureWidth(),
            m_texture_desc.GetTextureHeight(),
            1,
            RHIBufferType::Default,
            m_texture_desc.GetDataFormat(),
            RHIBufferResourceType::Tex2D
        };
    
    resource_manager.GetMemoryManager().AllocateBufferMemory(device, textureBufferDesc, m_texture_buffer);

    const size_t bytesPerPixel = GetBytePerPixelByFormat(m_texture_desc.GetDataFormat());
    const size_t imageBytesPerRow = bytesPerPixel * m_texture_desc.GetTextureWidth(); 
    const UINT64 textureUploadBufferSize = ((imageBytesPerRow + 255 ) & ~255) * (m_texture_desc.GetTextureHeight() - 1) + imageBytesPerRow;
    
    const RHIBufferDesc texture_upload_buffer_desc =
        {
            L"TextureBuffer_Upload",
            textureUploadBufferSize,
            1,
            1,
            RHIBufferType::Upload,
            m_texture_desc.GetDataFormat(),
            RHIBufferResourceType::Buffer
        };

    std::shared_ptr<IRHIBufferAllocation> m_texture_upload_buffer;
    resource_manager.GetMemoryManager().AllocateBufferMemory(device, texture_upload_buffer_desc, m_texture_upload_buffer);
    
    m_texture_upload_buffer->m_buffer->Transition(command_list, RHIResourceStateType::STATE_COPY_SOURCE);
    Transition(command_list, RHIResourceStateType::STATE_COPY_DEST);
    
    RETURN_IF_FALSE(DX12Utils::DX12Instance().UploadTextureDataToDefaultGPUBuffer(
        command_list,
        *m_texture_upload_buffer->m_buffer,
        *m_texture_buffer->m_buffer,
        texture_data,
        imageBytesPerRow,
        imageBytesPerRow * m_texture_desc.GetTextureHeight()))
    
    return true;
}

bool DX12Texture::Release(glTFRenderResourceManager& gl_tf_render_resource_manager)
{
    SAFE_RELEASE(m_raw_resource);
    return true;
}
