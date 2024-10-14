#pragma once
#include <vector>
#include <wincodec.h>

struct ImageLoadResult
{
    unsigned width;
    unsigned height;
    WICPixelFormatGUID format;
    std::vector<unsigned char> data;
};

class glTFImageLoader
{
public:
    static glTFImageLoader& Instance();
    bool LoadImageByFilename(const LPCWSTR filename, ImageLoadResult& result ) const;

protected:
    glTFImageLoader();
    void InitImageLoader();
    
    IWICImagingFactory* m_wicFactory;
};
