#include "IRHITexture.h"

RHITextureDesc::RHITextureDesc()
    : m_texture_data(nullptr)
    , m_texture_data_size(0)
    , m_texture_width(0)
    , m_texture_height(0)
    , m_texture_format(RHIDataFormat::Unknown)
{
    
}

bool RHITextureDesc::Init(unsigned width, unsigned height, RHIDataFormat format)
{
    assert(m_texture_data == nullptr);

    m_texture_width = width;
    m_texture_height = height;
    m_texture_format = format;

    m_texture_data_size = static_cast<size_t>(width * height * GetRHIDataFormatBitsPerPixel(format) / 8);
    m_texture_data.reset(static_cast<unsigned char*>(malloc(m_texture_data_size))) ;
    RETURN_IF_FALSE(m_texture_data)
    
    return true;
}
