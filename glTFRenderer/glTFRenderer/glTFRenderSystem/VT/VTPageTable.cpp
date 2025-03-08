#include "VTPageTable.h"

#include <vulkan/vulkan_core.h>

#include "RendererCommon.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIUtils.h"

bool VTPageTable::InitVTPageTable(int tex_id, int page_table_size, int page_size)
{
    m_tex_id = tex_id;
    m_page_table_size = page_table_size;
    m_quad_tree = std::make_shared<VTQuadTree>();
    const bool inited = m_quad_tree->InitQuadTree(page_table_size, page_size);
    GLTF_CHECK(inited);
    
    return true;
}

bool VTPageTable::InitRenderResource(glTFRenderResourceManager& resource_manager)
{
    RHITextureDesc page_table_texture_desc
    (
        "Page Table",
        m_page_table_size,
        m_page_table_size,
        RHIDataFormat::R16G16B16A16_UINT,
        static_cast<RHIResourceUsageFlags>(RUF_ALLOW_SRV | RUF_TRANSFER_DST),
        {
            RHIDataFormat::R8G8B8A8_UNORM,
            glm::vec4{0,0,0,0}
        }
    );
    
    const bool allocated = resource_manager.GetMemoryManager().AllocateTextureMemory(resource_manager.GetDevice(), resource_manager, page_table_texture_desc, m_page_texture);
    GLTF_CHECK(allocated);
    
    return true;
}

void VTPageTable::UpdateRenderResource(glTFRenderResourceManager& resource_manager)
{
    //RHIUtils::Instance().
}

bool VTPageTable::UpdatePage(const VTPage& page)
{
    GLTF_CHECK(page.tex == m_tex_id);
    
    int touch_level = m_quad_tree->GetLevel(page.mip);
    m_quad_tree->Touch(page.X, page.Y, touch_level);

    return true;
}

int VTPageTable::GetTextureId() const
{
    return m_tex_id;
}

const std::map<VTPage::HashType, VTPageTable::PhysicalPageOffset>& VTPageTable::GetPageTable() const
{
    return m_page_table;
}