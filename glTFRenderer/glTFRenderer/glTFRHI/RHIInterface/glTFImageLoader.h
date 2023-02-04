#pragma once
#include "wincodec.h"

struct RHITextureDesc;

class glTFImageLoader
{
public:
    glTFImageLoader();
    void InitImageLoader();
    bool LoadImageByFilename(const LPCWSTR filename, RHITextureDesc& desc ) const;

protected:
    IWICImagingFactory* m_wicFactory;
};
