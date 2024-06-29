#include "IRHITexture.h"

#include "SceneFileLoader/glTFImageLoader.h"

RHITextureDesc::RHITextureDesc()
    : m_texture_data(nullptr)
    , m_texture_data_size(0)
    , m_texture_width(0)
    , m_texture_height(0)
    , m_texture_format(RHIDataFormat::Unknown)
{
    
}

bool RHITextureDesc::Init(const ImageLoadResult& image_load_result)
{
    assert(m_texture_data == nullptr);

    m_texture_width = image_load_result.width;
    m_texture_height = image_load_result.height;
    m_texture_format = ConvertToRHIDataFormat(image_load_result.format);

    m_texture_data_size = image_load_result.data.size();
    m_texture_data.reset(static_cast<unsigned char*>(malloc(m_texture_data_size))) ;
    RETURN_IF_FALSE(m_texture_data)

    memcpy(m_texture_data.get(), image_load_result.data.data(), m_texture_data_size);
    
    return true;
}

RHIDataFormat RHITextureDesc::ConvertToRHIDataFormat(const WICPixelFormatGUID& wicFormatGUID)
{
    if (wicFormatGUID == GUID_WICPixelFormat128bppRGBAFloat) return RHIDataFormat::R32G32B32A32_FLOAT;
    else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBAHalf) return RHIDataFormat::R16G16B16A16_FLOAT;
    else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBA) return RHIDataFormat::R16G16B16A16_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBA) return RHIDataFormat::R8G8B8A8_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat32bppBGRA) return RHIDataFormat::B8G8R8A8_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat32bppBGR) return RHIDataFormat::B8G8R8X8_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBA1010102XR) return RHIDataFormat::R10G10B10_XR_BIAS_A2_UNORM;

    else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBA1010102) return RHIDataFormat::R10G10B10A2_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat16bppBGRA5551) return RHIDataFormat::B5G5R5A1_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat16bppBGR565) return RHIDataFormat::B5G6R5_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat32bppGrayFloat) return RHIDataFormat::R32_FLOAT;
    else if (wicFormatGUID == GUID_WICPixelFormat16bppGrayHalf) return RHIDataFormat::R16_FLOAT;
    else if (wicFormatGUID == GUID_WICPixelFormat16bppGray) return RHIDataFormat::R16_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat8bppGray) return RHIDataFormat::R8_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat8bppAlpha) return RHIDataFormat::A8_UNORM;

    else return RHIDataFormat::Unknown;
}
