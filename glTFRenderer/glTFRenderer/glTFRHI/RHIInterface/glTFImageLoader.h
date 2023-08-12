#pragma once
#include "wincodec.h"

struct RHITextureDesc;

class glTFImageLoader
{
public:
    static glTFImageLoader& Instance();
    bool LoadImageByFilename(const LPCWSTR filename, RHITextureDesc& desc ) const;

protected:
    glTFImageLoader();
    void InitImageLoader();
    
    IWICImagingFactory* m_wicFactory;
};
