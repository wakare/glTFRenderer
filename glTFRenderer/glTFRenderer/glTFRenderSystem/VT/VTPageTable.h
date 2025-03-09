#pragma once
#include <map>
#include <memory>

#include "VTCommon.h"
#include "VTQuadTree.h"
#include "glTFRHI/RHIInterface/IRHIMemoryManager.h"

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
    bool TouchPageAllocation(const VTPhysicalPageAllocationInfo& page_allocation);
    void UpdateTextureData();

    int GetTextureId() const;
    const std::map<VTPage::HashType, PhysicalPageOffset>& GetPageTable() const; 

    std::shared_ptr<IRHITextureAllocation> GetTextureAllocation() const;
    
protected:
    bool m_render_resource_init {false};
    int m_tex_id {-1};
    
    unsigned int m_page_table_size {0};
    unsigned int m_page_table_mip_count {0};
    
    std::shared_ptr<VTQuadTree> m_quad_tree;
    std::map<VTPage::HashType, VTPage> m_pages;
    std::map<VTPage::HashType, PhysicalPageOffset> m_page_table;

    std::shared_ptr<IRHITextureAllocation> m_page_texture;
    std::vector<std::shared_ptr<VTTextureData>> m_page_texture_datas;
};