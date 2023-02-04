#include "glTFImageLoader.h"

#include <cassert>
#include <wincodec.h>

#include "IRHITexture.h"
#include "RHICommon.h"

RHIDataFormat ConvertToRHIDataFormat(WICPixelFormatGUID& wicFormatGUID)
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

DXGI_FORMAT ConvertToDXGIFormat(WICPixelFormatGUID& wicFormatGUID)
{
    if (wicFormatGUID == GUID_WICPixelFormat128bppRGBAFloat) return DXGI_FORMAT_R32G32B32A32_FLOAT;
    else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBAHalf) return DXGI_FORMAT_R16G16B16A16_FLOAT;
    else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBA) return DXGI_FORMAT_R16G16B16A16_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBA) return DXGI_FORMAT_R8G8B8A8_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat32bppBGRA) return DXGI_FORMAT_B8G8R8A8_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat32bppBGR) return DXGI_FORMAT_B8G8R8X8_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBA1010102XR) return DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM;

    else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBA1010102) return DXGI_FORMAT_R10G10B10A2_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat16bppBGRA5551) return DXGI_FORMAT_B5G5R5A1_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat16bppBGR565) return DXGI_FORMAT_B5G6R5_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat32bppGrayFloat) return DXGI_FORMAT_R32_FLOAT;
    else if (wicFormatGUID == GUID_WICPixelFormat16bppGrayHalf) return DXGI_FORMAT_R16_FLOAT;
    else if (wicFormatGUID == GUID_WICPixelFormat16bppGray) return DXGI_FORMAT_R16_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat8bppGray) return DXGI_FORMAT_R8_UNORM;
    else if (wicFormatGUID == GUID_WICPixelFormat8bppAlpha) return DXGI_FORMAT_A8_UNORM;

    else return DXGI_FORMAT_UNKNOWN;
}

glTFImageLoader::glTFImageLoader()
    : m_wicFactory(nullptr)
{
}

void glTFImageLoader::InitImageLoader()
{
    assert(!m_wicFactory);

    if (!m_wicFactory)
    {
        THROW_IF_FAILED(CoInitialize(NULL))
        THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory,
            NULL,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&m_wicFactory)))
    }
    
}

bool glTFImageLoader::LoadImageByFilename(const LPCWSTR filename, RHITextureDesc& desc ) const
{
    IWICBitmapDecoder *wicDecoder = nullptr;
    THROW_IF_FAILED(m_wicFactory->CreateDecoderFromFilename(
        filename,                        // Image we want to load in
        NULL,                            // This is a vendor ID, we do not prefer a specific one so set to null
        GENERIC_READ,                    // We want to read from this file
        WICDecodeMetadataCacheOnLoad,    // We will cache the metadata right away, rather than when needed, which might be unknown
        &wicDecoder                      // the wic decoder to be created
        ))

    IWICBitmapFrameDecode* wicFrame = nullptr;

    THROW_IF_FAILED(wicDecoder->GetFrame(0, &wicFrame))
    
    WICPixelFormatGUID pixelFormat;
    THROW_IF_FAILED(wicFrame->GetPixelFormat(&pixelFormat))

    UINT textureWidth, textureHeight;
    THROW_IF_FAILED(wicFrame->GetSize(&textureWidth, &textureHeight))

    const RHIDataFormat dataFormat = ConvertToRHIDataFormat(pixelFormat);
    if (dataFormat == RHIDataFormat::Unknown)
    {
        // Unsupported format
        assert(false);
        return false;
    }

    RETURN_IF_FALSE(desc.Init(textureWidth, textureHeight, dataFormat))
    const unsigned bytesPerRow = textureWidth * GetRHIDataFormatBitsPerPixel(dataFormat) / 8;
    THROW_IF_FAILED(wicFrame->CopyPixels(0, bytesPerRow, desc.GetTextureDataSize(), static_cast<BYTE*>(desc.GetTextureData())))
    
    return true;
}
