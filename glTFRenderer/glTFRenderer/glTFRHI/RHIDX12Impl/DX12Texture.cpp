#include "DX12Texture.h"
#include "DX12Buffer.h"
#include "DX12Device.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIUtils.h"
#include "SceneFileLoader/glTFImageLoader.h"

ID3D12Resource* DX12Texture::GetRawResource() const
{
    return m_raw_resource ? m_raw_resource : dynamic_cast<const DX12Buffer&>(*m_texture_buffer->m_buffer).GetBuffer();
}

bool DX12Texture::InitFromExternalResource(ID3D12Resource* raw_resource, const RHITextureDesc& desc)
{
    m_raw_resource = raw_resource;
    m_texture_desc = desc;
    return true;
}

bool DX12Texture::InitTexture(IRHIDevice& device, const RHITextureDesc& desc)
{
    GLTF_CHECK(!desc.HasTextureData());
    
    m_texture_desc.Init(desc);
    const RHIBufferDesc textureBufferDesc =
        {
        L"TextureBuffer_Default",
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
    
    RETURN_IF_FALSE(glTFRenderResourceManager::GetMemoryManager().AllocateBufferMemory(device, textureBufferDesc, m_texture_buffer));
    
    return true;
}

bool DX12Texture::InitTextureAndUpload(IRHIDevice& device, IRHICommandList& command_list, const RHITextureDesc& desc)
{
    GLTF_CHECK(desc.HasTextureData());
    void* texture_data = desc.GetTextureData();
    
    m_texture_desc.Init(desc);
    
    const RHIBufferDesc textureBufferDesc =
        {
            L"TextureBuffer_Default",
            m_texture_desc.GetTextureWidth(),
            m_texture_desc.GetTextureHeight(),
            1,
            RHIBufferType::Default,
            m_texture_desc.GetDataFormat(),
            RHIBufferResourceType::Tex2D
        };
    
    glTFRenderResourceManager::GetMemoryManager().AllocateBufferMemory(device, textureBufferDesc, m_texture_buffer);

    const size_t bytesPerPixel = GetRHIDataFormatBitsPerPixel(m_texture_desc.GetDataFormat()) / 8;
    const size_t imageBytesPerRow = bytesPerPixel * m_texture_desc.GetTextureWidth(); 
    const UINT64 textureUploadBufferSize = ((imageBytesPerRow + 255 ) & ~255) * (m_texture_desc.GetTextureHeight() - 1) + imageBytesPerRow;
    
    const RHIBufferDesc textureUploadBufferDesc =
        {
            L"TextureBuffer_Upload",
            textureUploadBufferSize,
            1,
            1,
            RHIBufferType::Upload,
            m_texture_desc.GetDataFormat(),
            RHIBufferResourceType::Buffer
        };
    glTFRenderResourceManager::GetMemoryManager().AllocateBufferMemory(device, textureUploadBufferDesc, m_texture_upload_buffer);

    RETURN_IF_FALSE(RHIUtils::Instance().UploadTextureDataToDefaultGPUBuffer(
        command_list,
        *m_texture_upload_buffer->m_buffer,
        *m_texture_buffer->m_buffer,
        texture_data,
        imageBytesPerRow,
        imageBytesPerRow * m_texture_desc.GetTextureHeight()))

    // TODO: Texture source visible should be config by external parameters
    RETURN_IF_FALSE(RHIUtils::Instance().AddBufferBarrierToCommandList(
            command_list,
            *m_texture_buffer->m_buffer,
            RHIResourceStateType::STATE_COPY_DEST,
            RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE))

    return true;
}
