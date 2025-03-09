#pragma once
#include <cstdint>
#include <map>
#include <memory>

#include "VTCommon.h"
#include "VTQuadTree.h"
#include "glTFRHI/RHIInterface/IRHIMemoryManager.h"

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
        HashType hash = tex << 11 | mip << 8 | X << 4 | Y;
        return hash;
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

class VTPageTable
{
public:
    struct PhysicalPageOffset
    {
        int X;
        int Y;
    };

    bool InitVTPageTable(int tex_id, int page_table_size, int page_size);

    bool InitRenderResource(glTFRenderResourceManager& resource_manager);
    void UpdateRenderResource(glTFRenderResourceManager& resource_manager);

    void Invalidate();
    bool TouchPage(const VTPage& page);
    void UpdateTextureData();

    int GetTextureId() const;
    const std::map<VTPage::HashType, PhysicalPageOffset>& GetPageTable() const; 
    
protected:
    int m_tex_id {-1};
    unsigned int m_page_table_size {0};
    unsigned int m_page_table_mip_count {0};
    
    std::shared_ptr<VTQuadTree> m_quad_tree;
    std::map<VTPage::HashType, VTPage> m_pages;
    std::map<VTPage::HashType, PhysicalPageOffset> m_page_table;

    std::shared_ptr<IRHITextureAllocation> m_page_texture;
    std::vector<std::shared_ptr<VTTextureData>> m_page_texture_datas;
};