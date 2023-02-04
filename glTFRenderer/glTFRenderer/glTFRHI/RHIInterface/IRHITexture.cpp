#include "IRHITexture.h"

RHITextureDesc::RHITextureDesc()
    : m_textureData(nullptr)
    , m_textureDataSize(0)
    , m_textureWidth(0)
    , m_textureHeight(0)
    , m_textureFormat(RHIDataFormat::Unknown)
{
    
}

RHITextureDesc::~RHITextureDesc()
{
    if (m_textureData)
    {
        free(m_textureData);
        m_textureData = nullptr;
        m_textureDataSize = 0;
    }
}

bool RHITextureDesc::Init(unsigned width, unsigned height, RHIDataFormat format)
{
    assert(m_textureData == nullptr);

    m_textureWidth = width;
    m_textureHeight = height;
    m_textureFormat = format;

    m_textureDataSize = static_cast<size_t>(width * height * GetRHIDataFormatBitsPerPixel(format) / 8);
    m_textureData = malloc(m_textureDataSize);
    RETURN_IF_FALSE(m_textureData)
    
    return true;
}
