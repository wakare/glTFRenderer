#pragma once
#include <format>

#include "glTFRHI/RHIInterface/RHICommon.h"

struct VTPage
{
    using OffsetType = uint16_t;
    using MipType = uint8_t;
    using HashType = uint64_t;
    
    OffsetType X;
    OffsetType Y;
    MipType mip;
    int tex; // logical texture id 

    HashType PageHash() const
    {
        HashType hash = static_cast<HashType>(tex) << 40 | static_cast<HashType>(mip) << 32 | static_cast<HashType>(X) << 16 | Y;
        return hash;
    }

    std::string ToString() const
    {
        return std::format("PAGE{3}_MIP{2}_X{0}_Y{1}", X, Y, mip, tex);
    }

    bool operator==(const VTPage& rhs) const
    {
        return PageHash() == rhs.PageHash();
    }

    bool operator<(const VTPage& rhs) const
    {
        return PageHash() < rhs.PageHash();
    }
};

struct VTPhysicalPageAllocationInfo
{
    VTPage page;
    int X;
    int Y;
    std::shared_ptr<unsigned char[]> page_data;
};

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

    bool ResetTextureData()
    {
        memset(m_data.get(), 0, m_data_size);
        
        return true;
    }
    
    bool UpdateRegionData(int offset_x, int offset_y, int width, int height, const void* data)
    {
        GLTF_CHECK(0 <= offset_x && offset_x < m_width && 0 <= offset_y && offset_y < m_height);

        auto stride = GetBytePerPixelByFormat(m_format);
        
        for (int y = 0; y < height; ++y)
        {
            unsigned char* dst_data = m_data.get() + ((y + offset_y) * m_width + offset_x) * stride;
            const void* src_data = (unsigned char*)data + y * width * stride;
            memcpy(dst_data, src_data, width * stride);
        }

        return true;
    }

    bool UpdateRegionDataWithPixelData(int offset_x, int offset_y, int width, int height, const void* pixel_data, size_t pixel_data_size)
    {
        GLTF_CHECK(0 <= offset_x && offset_x < m_width && 0 <= offset_y && offset_y < m_height);

        auto stride = GetBytePerPixelByFormat(m_format);
        GLTF_CHECK(pixel_data_size == stride);
        
        for (int y = offset_y; y < offset_y + height; ++y)
        {
            for (int x = offset_x; x < offset_x + width; ++x)
            {
                unsigned char* dst_data = m_data.get() + (y * m_width + x) * stride;
                memcpy(dst_data, pixel_data, stride);
            }
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

ALIGN_FOR_CBV_STRUCT struct VTLogicalTextureInfo
{
    inline static std::string Name = "VT_LOGICAL_TEXTURE_INFO_REGISTER_INDEX";
    
    unsigned feed_back_tex_index;
    unsigned page_table_tex_index;
    unsigned logical_texture_size;
    unsigned padding;
};

