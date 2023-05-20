#include "DX12Texture.h"

#include <codecvt>

#include "DX12Device.h"
#include "../RHIResourceFactory.h"
#include "../RHIUtils.h"
#include "../RHIInterface/glTFImageLoader.h"

DX12Texture::DX12Texture()
= default;

DX12Texture::~DX12Texture()
{
}

bool DX12Texture::UploadTextureFromFile(IRHIDevice& device, IRHICommandList& commandList, glTFImageLoader& imageLoader, const std::string& filePath)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    const std::wstring convertPath = converter.from_bytes(filePath);
    RETURN_IF_FALSE(imageLoader.LoadImageByFilename(convertPath.c_str(), m_textureDesc))

    const RHIBufferDesc textureBufferDesc = {L"TextureBuffer_Default", m_textureDesc.GetTextureWidth(), m_textureDesc.GetTextureHeight(), 1,  RHIBufferType::Default, m_textureDesc.GetDataFormat(), RHIBufferResourceType::Tex2D};
    m_textureBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    RETURN_IF_FALSE(m_textureBuffer->InitGPUBuffer(device, textureBufferDesc))

    const size_t bytesPerPixel = GetRHIDataFormatBitsPerPixel(m_textureDesc.GetDataFormat()) / 8;
    const size_t imageBytesPerRow = bytesPerPixel * m_textureDesc.GetTextureWidth(); 
    const UINT64 textureUploadBufferSize = ((imageBytesPerRow + 255 ) & ~255) * (m_textureDesc.GetTextureHeight() - 1) + imageBytesPerRow;
    
    const RHIBufferDesc textureUploadBufferDesc = {L"TextureBuffer_Upload", textureUploadBufferSize, 1, 1,  RHIBufferType::Upload, m_textureDesc.GetDataFormat(), RHIBufferResourceType::Buffer};
    m_textureUploadBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    RETURN_IF_FALSE(m_textureUploadBuffer->InitGPUBuffer(device, textureUploadBufferDesc))

    RETURN_IF_FALSE(RHIUtils::Instance().UploadTextureDataToDefaultGPUBuffer(commandList, *m_textureUploadBuffer, *m_textureBuffer, m_textureDesc.GetTextureData(), imageBytesPerRow, imageBytesPerRow * m_textureDesc.GetTextureHeight()))

    // TODO: Texture source visible should be config by external parameters
    RETURN_IF_FALSE(RHIUtils::Instance().AddBufferBarrierToCommandList(commandList, *m_textureBuffer, RHIResourceStateType::COPY_DEST, RHIResourceStateType::PIXEL_SHADER_RESOURCE))

    return true;
}

IRHIGPUBuffer& DX12Texture::GetGPUBuffer()
{
    return *m_textureBuffer;
}
