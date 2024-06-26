#include "glTFImageLoader.h"

#include <cassert>
#include <wincodec.h>

#include "glTFRHI/RHIInterface/RHICommon.h"
#include "RendererCommon.h"

RHIDataFormat ConvertToRHIDataFormat(const WICPixelFormatGUID& wicFormatGUID)
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

// get a dxgi compatible wic format from another wic format
WICPixelFormatGUID GetConvertToWICFormat(const WICPixelFormatGUID& wicFormatGUID)
{
    if (wicFormatGUID == GUID_WICPixelFormatBlackWhite) return GUID_WICPixelFormat8bppGray;
    else if (wicFormatGUID == GUID_WICPixelFormat1bppIndexed) return GUID_WICPixelFormat32bppRGBA;
    else if (wicFormatGUID == GUID_WICPixelFormat2bppIndexed) return GUID_WICPixelFormat32bppRGBA;
    else if (wicFormatGUID == GUID_WICPixelFormat4bppIndexed) return GUID_WICPixelFormat32bppRGBA;
    else if (wicFormatGUID == GUID_WICPixelFormat8bppIndexed) return GUID_WICPixelFormat32bppRGBA;
    else if (wicFormatGUID == GUID_WICPixelFormat2bppGray) return GUID_WICPixelFormat8bppGray;
    else if (wicFormatGUID == GUID_WICPixelFormat4bppGray) return GUID_WICPixelFormat8bppGray;
    else if (wicFormatGUID == GUID_WICPixelFormat16bppGrayFixedPoint) return GUID_WICPixelFormat16bppGrayHalf;
    else if (wicFormatGUID == GUID_WICPixelFormat32bppGrayFixedPoint) return GUID_WICPixelFormat32bppGrayFloat;
    else if (wicFormatGUID == GUID_WICPixelFormat16bppBGR555) return GUID_WICPixelFormat16bppBGRA5551;
    else if (wicFormatGUID == GUID_WICPixelFormat32bppBGR101010) return GUID_WICPixelFormat32bppRGBA1010102;
    else if (wicFormatGUID == GUID_WICPixelFormat24bppBGR) return GUID_WICPixelFormat32bppRGBA;
    else if (wicFormatGUID == GUID_WICPixelFormat24bppRGB) return GUID_WICPixelFormat32bppRGBA;
    else if (wicFormatGUID == GUID_WICPixelFormat32bppPBGRA) return GUID_WICPixelFormat32bppRGBA;
    else if (wicFormatGUID == GUID_WICPixelFormat32bppPRGBA) return GUID_WICPixelFormat32bppRGBA;
    else if (wicFormatGUID == GUID_WICPixelFormat48bppRGB) return GUID_WICPixelFormat64bppRGBA;
    else if (wicFormatGUID == GUID_WICPixelFormat48bppBGR) return GUID_WICPixelFormat64bppRGBA;
    else if (wicFormatGUID == GUID_WICPixelFormat64bppBGRA) return GUID_WICPixelFormat64bppRGBA;
    else if (wicFormatGUID == GUID_WICPixelFormat64bppPRGBA) return GUID_WICPixelFormat64bppRGBA;
    else if (wicFormatGUID == GUID_WICPixelFormat64bppPBGRA) return GUID_WICPixelFormat64bppRGBA;
    else if (wicFormatGUID == GUID_WICPixelFormat48bppRGBFixedPoint) return GUID_WICPixelFormat64bppRGBAHalf;
    else if (wicFormatGUID == GUID_WICPixelFormat48bppBGRFixedPoint) return GUID_WICPixelFormat64bppRGBAHalf;
    else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBAFixedPoint) return GUID_WICPixelFormat64bppRGBAHalf;
    else if (wicFormatGUID == GUID_WICPixelFormat64bppBGRAFixedPoint) return GUID_WICPixelFormat64bppRGBAHalf;
    else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBFixedPoint) return GUID_WICPixelFormat64bppRGBAHalf;
    else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBHalf) return GUID_WICPixelFormat64bppRGBAHalf;
    else if (wicFormatGUID == GUID_WICPixelFormat48bppRGBHalf) return GUID_WICPixelFormat64bppRGBAHalf;
    else if (wicFormatGUID == GUID_WICPixelFormat128bppPRGBAFloat) return GUID_WICPixelFormat128bppRGBAFloat;
    else if (wicFormatGUID == GUID_WICPixelFormat128bppRGBFloat) return GUID_WICPixelFormat128bppRGBAFloat;
    else if (wicFormatGUID == GUID_WICPixelFormat128bppRGBAFixedPoint) return GUID_WICPixelFormat128bppRGBAFloat;
    else if (wicFormatGUID == GUID_WICPixelFormat128bppRGBFixedPoint) return GUID_WICPixelFormat128bppRGBAFloat;
    else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBE) return GUID_WICPixelFormat128bppRGBAFloat;
    else if (wicFormatGUID == GUID_WICPixelFormat32bppCMYK) return GUID_WICPixelFormat32bppRGBA;
    else if (wicFormatGUID == GUID_WICPixelFormat64bppCMYK) return GUID_WICPixelFormat64bppRGBA;
    else if (wicFormatGUID == GUID_WICPixelFormat40bppCMYKAlpha) return GUID_WICPixelFormat64bppRGBA;
    else if (wicFormatGUID == GUID_WICPixelFormat80bppCMYKAlpha) return GUID_WICPixelFormat64bppRGBA;

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
    else if (wicFormatGUID == GUID_WICPixelFormat32bppRGB) return GUID_WICPixelFormat32bppRGBA;
    else if (wicFormatGUID == GUID_WICPixelFormat64bppRGB) return GUID_WICPixelFormat64bppRGBA;
    else if (wicFormatGUID == GUID_WICPixelFormat64bppPRGBAHalf) return GUID_WICPixelFormat64bppRGBAHalf;
#endif

    else return GUID_WICPixelFormatDontCare;
}

glTFImageLoader& glTFImageLoader::Instance()
{
    static glTFImageLoader _instance;
    return _instance;
}

glTFImageLoader::glTFImageLoader()
    : m_wicFactory(nullptr)
{
    InitImageLoader();
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
    IWICFormatConverter* wicConverter = nullptr;
    
    THROW_IF_FAILED(wicDecoder->GetFrame(0, &wicFrame))
    
    WICPixelFormatGUID pixelFormat;
    THROW_IF_FAILED(wicFrame->GetPixelFormat(&pixelFormat))

    UINT textureWidth, textureHeight;
    THROW_IF_FAILED(wicFrame->GetSize(&textureWidth, &textureHeight))

    RHIDataFormat dataFormat = ConvertToRHIDataFormat(pixelFormat);
    const bool needConvertFormat = dataFormat == RHIDataFormat::Unknown;
    if (needConvertFormat)
    {
        // Unsupported format, try convert
        const WICPixelFormatGUID targetConvertFormat = GetConvertToWICFormat(pixelFormat);
        if (targetConvertFormat == GUID_WICPixelFormatDontCare)
        {
            // Can not convert to available format
            assert(false);
            return false;
        }
        
        
        THROW_IF_FAILED(m_wicFactory->CreateFormatConverter(&wicConverter))

        BOOL canConvert = false;
        THROW_IF_FAILED(wicConverter->CanConvert(pixelFormat, targetConvertFormat, &canConvert))
        if (!canConvert)
        {
            assert(false);
            return false;
        }

        THROW_IF_FAILED(wicConverter->Initialize(wicFrame, targetConvertFormat,  WICBitmapDitherTypeErrorDiffusion, 0, 0, WICBitmapPaletteTypeCustom))
        dataFormat = ConvertToRHIDataFormat(targetConvertFormat);
        
        assert(needConvertFormat && wicConverter);
    }

    RETURN_IF_FALSE(desc.Init(textureWidth, textureHeight, dataFormat))
    const unsigned bytesPerRow = textureWidth * GetRHIDataFormatBitsPerPixel(dataFormat) / 8;
    if (needConvertFormat)
    {
        THROW_IF_FAILED(wicConverter->CopyPixels(0, bytesPerRow, desc.GetTextureDataSize(), desc.GetTextureData()))
    }
    else
    {
        THROW_IF_FAILED(wicFrame->CopyPixels(0, bytesPerRow, desc.GetTextureDataSize(), desc.GetTextureData()))
    }

    return true;
}
