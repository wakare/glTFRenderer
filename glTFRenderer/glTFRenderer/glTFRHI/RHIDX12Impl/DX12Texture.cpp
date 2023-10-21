#include "DX12Texture.h"
#include "DX12Device.h"
#include "glTFRHI/RHIResourceFactory.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFUtils/glTFImageLoader.h"
#include "glTFUtils/glTFUtils.h"

DX12Texture::DX12Texture()
= default;

DX12Texture::~DX12Texture()
= default;

bool DX12Texture::UploadTextureFromFile(IRHIDevice& device, IRHICommandList& commandList, const std::string& filePath, bool srgb)
{
    const std::wstring convertPath = to_wide_string(filePath);
    RETURN_IF_FALSE(glTFImageLoader::Instance().LoadImageByFilename(convertPath.c_str(), m_textureDesc))

    // Handle srgb case
    if (srgb)
    {
        m_textureDesc.SetDataFormat(ConvertToSRGBFormat(m_textureDesc.GetDataFormat()));
    }
    
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
    RETURN_IF_FALSE(RHIUtils::Instance().AddBufferBarrierToCommandList(commandList, *m_textureBuffer, RHIResourceStateType::STATE_COPY_DEST, RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE))

    return true;
}

IRHIGPUBuffer& DX12Texture::GetGPUBuffer()
{
    return *m_textureBuffer;
}
