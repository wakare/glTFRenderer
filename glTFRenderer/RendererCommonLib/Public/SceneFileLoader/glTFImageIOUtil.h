#pragma once
#include <vector>
#include <wincodec.h>
#include <string>

struct ImageLoadResult
{
    unsigned width;
    unsigned height;
    WICPixelFormatGUID format;
    std::vector<unsigned char> data;
};

class glTFImageIOUtil
{
public:
    static glTFImageIOUtil& Instance();
    bool LoadImageByFilename(const LPCWSTR filename, ImageLoadResult& result ) const;

    bool SaveAsPNG(const std::string& file_name, const void* data, int width, int height) const;

protected:
    glTFImageIOUtil();
    void InitImageLoader();
    
    IWICImagingFactory* m_wicFactory;
};
