#include "VTPageTable.h"

#include <vulkan/vulkan_core.h>

#include "RendererCommon.h"
#include "VTTextureTypes.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIUtils.h"

bool VTPageTable::InitVTPageTable(int tex_id, int page_table_size, int page_size)
{
    m_tex_id = tex_id;
    m_page_size = page_size;
    m_page_table_size = page_table_size;
    m_page_table_mip_count = 1 + log2(m_page_table_size);
    
    m_quad_tree = std::make_shared<VTQuadTree>();
    const bool inited = m_quad_tree->InitQuadTree(page_table_size, page_size);
    GLTF_CHECK(inited);

    m_page_texture_datas.resize(m_page_table_mip_count);
    unsigned int mip_size = m_page_table_size;
    for (unsigned int i = 0; i < m_page_table_mip_count; i++)
    {
        m_page_texture_datas[i] = std::make_shared<VTTextureData>(mip_size, mip_size, RHIDataFormat::R16G16B16A16_UINT);
        mip_size = mip_size >> 1;
    }
    
    return true;
}

bool VTPageTable::InitRenderResource(glTFRenderResourceManager& resource_manager)
{
    if (m_render_resource_init)
    {
        return true;
    }

    m_render_resource_init = true;
    RHITextureDesc page_table_texture_desc
    (
        "Page Table",
        m_page_table_size,
        m_page_table_size,
        RHIDataFormat::R16G16B16A16_UINT,
        static_cast<RHIResourceUsageFlags>(RUF_ALLOW_SRV | RUF_TRANSFER_DST | RUF_CONTAINS_MIPMAP),
        {
            RHIDataFormat::R16G16B16A16_UINT,
            glm::vec4{0,0,0,0}
        }
    );
    
    const bool allocated = resource_manager.GetMemoryManager().AllocateTextureMemory(resource_manager.GetDevice(), resource_manager, page_table_texture_desc, m_page_texture);
    GLTF_CHECK(allocated);
    
    return true;
}

void VTPageTable::UpdateRenderResource(glTFRenderResourceManager& resource_manager)
{
    if (!m_page_texture)
    {
        InitRenderResource(resource_manager);
    }

    unsigned mip = 0;
    for (const auto& texture_data : m_page_texture_datas)
    {
        const RHITextureMipUploadInfo upload_info
        {
            {
                texture_data->GetData(),
                texture_data->GetDataSize()
            },
            mip++,
        };
    
        RHIUtils::Instance().UploadTextureMipData(resource_manager.GetCommandListForRecord(), resource_manager.GetMemoryManager(), resource_manager.GetDevice(), *m_page_texture->m_texture, upload_info );    
    }
}

void VTPageTable::Invalidate()
{
    m_quad_tree->Invalidate();
}

bool VTPageTable::TouchPageAllocation(const VTPhysicalPageAllocationInfo& page_allocation)
{
    GLTF_CHECK(page_allocation.page.tex == m_tex_id);
    
    //int touch_level = m_quad_tree->GetLevel(page_allocation.page.mip);
    int touch_level = page_allocation.page.mip;
    m_quad_tree->Touch(page_allocation.page.X, page_allocation.page.Y, page_allocation.X, page_allocation.Y, touch_level);

    return true;
}

void VTPageTable::UpdateTextureData()
{
    struct PixelRGBA16 {
        uint16_t r;
        uint16_t g;
        uint16_t b;
        uint16_t a;
    };
    
    for (int i = 0; i < m_page_texture_datas.size(); i++)
    {
        auto& texture_data = m_page_texture_datas[i];
        auto update_data = [&, this](const QuadTreeNode& node)
        {
            if (!node.IsValid())
            {
                return;
            }

            if (node.GetLevel() < i)
            {
                return;
            }

            PixelRGBA16 pixel_data
            {
                static_cast<uint16_t>(node.GetPageX()),
                static_cast<uint16_t>(node.GetPageY()),
                static_cast<uint16_t>(node.GetLevel()),
                1
            };

            int offset_x    = (node.GetX()       / m_page_size) >> i;
            int offset_y    = (node.GetY()       / m_page_size) >> i;
            int width       = (node.GetWidth()   / m_page_size) >> i;
            int height      = (node.GetHeight()  / m_page_size) >> i;

            //LOG_FORMAT_FLUSH("UpdateRegionDataWithPixelData Node:%s with mip %d, [%d, %d, %d, %d] data[%d, %d, %d, %d]\n", node.ToString().c_str(), i, offset_x, offset_y, width, height, pixel_data.r, pixel_data.g, pixel_data.b, pixel_data.a);
            texture_data->UpdateRegionDataWithPixelData(offset_x, offset_y,
                width, height, &pixel_data, sizeof(PixelRGBA16));
        };

        m_quad_tree->TraverseLambda(update_data);    
    }
}

int VTPageTable::GetTextureId() const
{
    return m_tex_id;
}

const std::map<VTPage::HashType, VTPageTable::PhysicalPageOffset>& VTPageTable::GetPageTable() const
{
    return m_page_table;
}

std::shared_ptr<IRHITextureAllocation> VTPageTable::GetTextureAllocation() const
{
    return m_page_texture;
}
