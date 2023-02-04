#include "DX12Texture.h"

#include "DX12Device.h"
#include "../RHIResourceFactory.h"
#include "../RHIUtils.h"
#include "../RHIInterface/glTFImageLoader.h"

DX12Texture::DX12Texture()
{
}

DX12Texture::~DX12Texture()
{
}

bool DX12Texture::UploadTextureFromFile(IRHIDevice& device, IRHICommandList& commandList, glTFImageLoader& imageLoader, const std::wstring& filePath)
{
    RETURN_IF_FALSE(imageLoader.LoadImageByFilename(filePath.c_str(), m_textureDesc))

    const RHIBufferDesc textureBufferDesc = {L"TextureBuffer_Default", m_textureDesc.GetTextureWidth(), m_textureDesc.GetTextureHeight(), 1,  RHIBufferType::Default, m_textureDesc.GetDataFormat(), RHIBufferResourceType::Tex2D};
    m_textureBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    RETURN_IF_FALSE(m_textureBuffer->InitGPUBuffer(device, textureBufferDesc))

    const RHIBufferDesc textureUploadBufferDesc = {L"TextureBuffer_Upload", m_textureDesc.GetTextureWidth(), m_textureDesc.GetTextureHeight(), 1,  RHIBufferType::Upload, m_textureDesc.GetDataFormat(), RHIBufferResourceType::Tex2D};
    m_textureUploadBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    RETURN_IF_FALSE(m_textureUploadBuffer->InitGPUBuffer(device, textureBufferDesc))

    RETURN_IF_FALSE(RHIUtils::Instance().UploadDataToDefaultGPUBuffer(commandList, *m_textureUploadBuffer, *m_textureBuffer, m_textureDesc.GetTextureData(), m_textureDesc.GetTextureDataSize()))

    // TODO: Texture source visible should be config by external parameters
    RETURN_IF_FALSE(RHIUtils::Instance().AddBufferBarrierToCommandList(commandList, *m_textureBuffer, RHIResourceStateType::COPY_DEST, RHIResourceStateType::PIXEL_SHADER_RESOURCE))

    return true;
}

IRHIGPUBuffer& DX12Texture::GetGPUBuffer()
{
    return *m_textureBuffer;
}
