#pragma once
#include "glTFRHI/RHIInterface/RHICommon.h"

// single mip texture data
class VTTextureData
{
public:
    VTTextureData(int width, int height, RHIDataFormat format)
        : m_width(width), m_height(height), m_format(format)
    {
        auto stride = GetBytePerPixelByFormat(m_format);
        m_data_size = stride * m_width * m_height;
        m_data = std::make_shared<unsigned char[]>(m_data_size);
    }

    bool UpdateRegionData(int offset_x, int offset_y, int width, int height, const void* data)
    {
        GLTF_CHECK(0 <= offset_x && offset_x < width && 0 <= offset_y && offset_y < height);

        auto stride = GetBytePerPixelByFormat(m_format);
        
        for (int y = offset_y; y < offset_y + height; ++y)
        {
            unsigned char* dst_data = m_data.get() + (y * m_width + offset_x) * stride;
            const void* src_data = (unsigned char*)data + y * width * stride;
            memcpy(dst_data, src_data, width * stride);
        }

        return true;
    }

    std::shared_ptr<unsigned char[]> GetData()
    {
        return m_data;
    }

    size_t GetDataSize() const
    {
        return m_data_size;
    }
    
protected:
    int m_width;
    int m_height;
    RHIDataFormat m_format;

    std::shared_ptr<unsigned char[]> m_data;
    size_t m_data_size;
};
