#include "SceneFileLoader/glTFImageIOUtil.h"

#include <cassert>
#include <Shlwapi.h>
#include <wincodec.h>
#include <wrl/client.h>

#include "RendererCommon.h"

using Microsoft::WRL::ComPtr;

bool EnsureDirectoryExists(const std::wstring& directory) {
    // 将相对路径转换为绝对路径
    wchar_t fullPath[MAX_PATH];
    if (GetFullPathNameW(directory.c_str(), MAX_PATH, fullPath, nullptr) == 0) {
        LOG_FORMAT_FLUSH("Error: Failed to convert to full path!\n");
        return false;
    }

    // 移除文件名，只保留路径
    if (!PathRemoveFileSpecW(fullPath)) {
        return false;
    }
    
    std::wstring path(fullPath);
    
    // 检查路径是否已经存在
    DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return true;  // 目录已存在
    }

    // 递归创建父目录
    size_t lastSlash = path.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        std::wstring parentPath = path.substr(0, lastSlash);
        if (!EnsureDirectoryExists(parentPath)) {
            return false;
        }
    }

    // 创建当前目录
    if (!CreateDirectoryW(path.c_str(), NULL)) {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS) {
            LOG_FORMAT_FLUSH("Error: Failed to create directory '%ls'!\n", path.c_str())
            return false;
        }
    }

    return true;
}


#define GUID_MATCH_RETURN_FALSE(x) if (wicFormatGUID == (x)) return false;
bool NeedConvertFormat(const WICPixelFormatGUID& wicFormatGUID)
{
    GUID_MATCH_RETURN_FALSE(GUID_WICPixelFormat128bppRGBAFloat)
    GUID_MATCH_RETURN_FALSE(GUID_WICPixelFormat64bppRGBAHalf)
    GUID_MATCH_RETURN_FALSE(GUID_WICPixelFormat64bppRGBA)
    GUID_MATCH_RETURN_FALSE(GUID_WICPixelFormat32bppRGBA)
    GUID_MATCH_RETURN_FALSE(GUID_WICPixelFormat32bppBGRA)
    GUID_MATCH_RETURN_FALSE(GUID_WICPixelFormat32bppBGR)
    GUID_MATCH_RETURN_FALSE(GUID_WICPixelFormat32bppRGBA1010102XR)
    GUID_MATCH_RETURN_FALSE(GUID_WICPixelFormat32bppRGBA1010102)
    GUID_MATCH_RETURN_FALSE(GUID_WICPixelFormat32bppGrayFloat)
    GUID_MATCH_RETURN_FALSE(GUID_WICPixelFormat16bppBGRA5551)
    GUID_MATCH_RETURN_FALSE(GUID_WICPixelFormat16bppBGR565)
    GUID_MATCH_RETURN_FALSE(GUID_WICPixelFormat16bppGrayHalf)
    GUID_MATCH_RETURN_FALSE(GUID_WICPixelFormat16bppGray)
    GUID_MATCH_RETURN_FALSE(GUID_WICPixelFormat8bppGray)
    GUID_MATCH_RETURN_FALSE(GUID_WICPixelFormat8bppAlpha)
    
    return true;
}
#undef GUID_MATCH_RETURN_FALSE

#define PIXEL_FORMAT_MATCH_RETURN_BYTE_COUNT(x, byte) if (wicFormatGUID == (x)) return byte;
unsigned GetPixelFormatByte(const WICPixelFormatGUID& wicFormatGUID)
{
    PIXEL_FORMAT_MATCH_RETURN_BYTE_COUNT(GUID_WICPixelFormat128bppRGBAFloat, 16)
    PIXEL_FORMAT_MATCH_RETURN_BYTE_COUNT(GUID_WICPixelFormat64bppRGBAHalf, 8)
    PIXEL_FORMAT_MATCH_RETURN_BYTE_COUNT(GUID_WICPixelFormat64bppRGBA, 8)
    PIXEL_FORMAT_MATCH_RETURN_BYTE_COUNT(GUID_WICPixelFormat32bppRGBA, 4)
    PIXEL_FORMAT_MATCH_RETURN_BYTE_COUNT(GUID_WICPixelFormat32bppBGRA, 4)
    PIXEL_FORMAT_MATCH_RETURN_BYTE_COUNT(GUID_WICPixelFormat32bppBGR, 4)
    PIXEL_FORMAT_MATCH_RETURN_BYTE_COUNT(GUID_WICPixelFormat32bppRGBA1010102XR, 4)
    PIXEL_FORMAT_MATCH_RETURN_BYTE_COUNT(GUID_WICPixelFormat32bppRGBA1010102, 4)
    PIXEL_FORMAT_MATCH_RETURN_BYTE_COUNT(GUID_WICPixelFormat32bppGrayFloat, 4)
    PIXEL_FORMAT_MATCH_RETURN_BYTE_COUNT(GUID_WICPixelFormat16bppBGRA5551, 2)
    PIXEL_FORMAT_MATCH_RETURN_BYTE_COUNT(GUID_WICPixelFormat16bppBGR565, 2)
    PIXEL_FORMAT_MATCH_RETURN_BYTE_COUNT(GUID_WICPixelFormat16bppGrayHalf, 2)
    PIXEL_FORMAT_MATCH_RETURN_BYTE_COUNT(GUID_WICPixelFormat16bppGray, 2)
    PIXEL_FORMAT_MATCH_RETURN_BYTE_COUNT(GUID_WICPixelFormat8bppGray, 16)
    PIXEL_FORMAT_MATCH_RETURN_BYTE_COUNT(GUID_WICPixelFormat8bppAlpha, 16)
    
    GLTF_CHECK(false);
    return 0;
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

glTFImageIOUtil& glTFImageIOUtil::Instance()
{
    static glTFImageIOUtil _instance;
    return _instance;
}

glTFImageIOUtil::glTFImageIOUtil()
    : m_wicFactory(nullptr)
{
    InitImageLoader();
}

void glTFImageIOUtil::InitImageLoader()
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

bool glTFImageIOUtil::LoadImageByFilename(const LPCWSTR filename, ImageLoadResult& desc ) const
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
    
    WICPixelFormatGUID pixel_format;
    THROW_IF_FAILED(wicFrame->GetPixelFormat(&pixel_format))

    UINT texture_width, texture_height;
    THROW_IF_FAILED(wicFrame->GetSize(&texture_width, &texture_height))

    const bool needConvertFormat = NeedConvertFormat(pixel_format);
    if (needConvertFormat)
    {
        // Unsupported format, try convert
        const WICPixelFormatGUID targetConvertFormat = GetConvertToWICFormat(pixel_format);
        if (targetConvertFormat == GUID_WICPixelFormatDontCare)
        {
            // Can not convert to available format
            assert(false);
            return false;
        }
        
        THROW_IF_FAILED(m_wicFactory->CreateFormatConverter(&wicConverter))

        BOOL canConvert = false;
        THROW_IF_FAILED(wicConverter->CanConvert(pixel_format, targetConvertFormat, &canConvert))
        if (!canConvert)
        {
            assert(false);
            return false;
        }

        THROW_IF_FAILED(wicConverter->Initialize(wicFrame, targetConvertFormat,  WICBitmapDitherTypeErrorDiffusion, 0, 0, WICBitmapPaletteTypeCustom))
        
        assert(needConvertFormat && wicConverter);
        pixel_format = targetConvertFormat;
    }

    desc.width = texture_width;
    desc.height = texture_height;
    desc.format = pixel_format;
    desc.data.resize(texture_width * texture_height * GetPixelFormatByte(pixel_format));
    
    const unsigned bytesPerRow = texture_width * GetPixelFormatByte(pixel_format);
    if (needConvertFormat)
    {
        THROW_IF_FAILED(wicConverter->CopyPixels(0, bytesPerRow, desc.data.size(), desc.data.data()))
    }
    else
    {
        THROW_IF_FAILED(wicFrame->CopyPixels(0, bytesPerRow, desc.data.size(), desc.data.data()))
    }

    return true;
}

bool glTFImageIOUtil::SaveAsPNG(const std::string& file_name, const void* image_data, int width, int height) const
{
    if (!image_data || width <= 0 || height <= 0) {
        LOG_FORMAT_FLUSH("Error: Invalid image data or dimensions!\n")
        return false;
    }

    // 确保目录存在
    const std::wstring wide_file_name = to_wide_string(file_name);
    if (!EnsureDirectoryExists(wide_file_name)) {
        LOG_FORMAT_FLUSH("Error: Failed to create directory for %s\n", file_name.c_str());
        return false;
    }

    // 创建 WIC 编码器
    ComPtr<IWICBitmapEncoder> pEncoder;
    HRESULT hr = m_wicFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &pEncoder);
    if (FAILED(hr)) {
        LOG_FORMAT_FLUSH("Error: Failed to create WIC encoder!\n")
        return false;
    }

    // 创建文件流
    ComPtr<IWICStream> pStream;
    hr = m_wicFactory->CreateStream(&pStream);
    if (FAILED(hr)) {
        LOG_FORMAT_FLUSH("Error: Failed to create WIC stream!\n")        
        return false;
    }
    hr = pStream->InitializeFromFilename(wide_file_name.c_str(), GENERIC_WRITE);
    if (FAILED(hr)) {
        LOG_FORMAT_FLUSH("Error: Failed to initialize WIC stream from file!\n")
        return false;
    }

    // 绑定编码器到文件流
    hr = pEncoder->Initialize(pStream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)) {
        LOG_FORMAT_FLUSH("Error: Failed to initialize WIC encoder!\n")
        return false;
    }

    // 创建编码器帧
    ComPtr<IWICBitmapFrameEncode> pFrame;
    ComPtr<IPropertyBag2> pEncoderOptions;
    hr = pEncoder->CreateNewFrame(&pFrame, &pEncoderOptions);
    if (FAILED(hr)) {
        LOG_FORMAT_FLUSH("Error: Failed to create new frame in WIC encoder!\n")
        return false;
    }
    hr = pFrame->Initialize(pEncoderOptions.Get());
    if (FAILED(hr)) {
        LOG_FORMAT_FLUSH("Error: Failed to initialize WIC frame!\n")
        return false;
    }

    // 设置帧尺寸
    hr = pFrame->SetSize(width, height);
    if (FAILED(hr)) {
        LOG_FORMAT_FLUSH("Error: Failed to set frame size!\n");
        return false;
    }

    // 设置像素格式
    WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat32bppRGBA;
    hr = pFrame->SetPixelFormat(&pixelFormat);
    if (FAILED(hr)) {
        LOG_FORMAT_FLUSH("Error: Failed to set pixel format!\n")
        return false;
    }

    // 写入像素数据
    hr = pFrame->WritePixels(height, width * 4, width * height * 4, (BYTE*)image_data);
    if (FAILED(hr)) {
        LOG_FORMAT_FLUSH("Error: Failed to write pixel data!\n")
        return false;
    }

    // 结束帧并保存
    hr = pFrame->Commit();
    if (FAILED(hr)) {
        LOG_FORMAT_FLUSH("Error: Failed to commit frame!\n")
        return false;
    }
    hr = pEncoder->Commit();
    if (FAILED(hr)) {
        LOG_FORMAT_FLUSH("Error: Failed to commit encoder!\n");
        return false;
    }

    LOG_FORMAT_FLUSH("Image successfully saved to: %s\n", file_name.c_str())
    return true;
}
