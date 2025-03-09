#include "VTTextureTypes.h"

#include <utility>

#include "VirtualTextureSystem.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIUtils.h"

bool VTLogicalTexture::InitLogicalTexture(const RHITextureDesc& desc)
{
    GLTF_CHECK(desc.GetTextureWidth() > 0 && desc.GetTextureHeight() > 0 && desc.GetTextureWidth() == desc.GetTextureHeight());

    static unsigned _inner_texture_id = 0;
    m_texture_id = _inner_texture_id++;
    m_size = desc.GetTextureWidth();

    m_texture_data = desc.GetTextureData();
    m_texture_data_size = desc.GetTextureDataSize();
    
    return true;
}

int VTLogicalTexture::GetTextureId() const
{
    return m_texture_id;
}

int VTLogicalTexture::GetSize() const
{
    return m_size;
}

void VTPageLRU::AddPage(const VTPage& page)
{
    if (m_pages.contains(page))
    {
        return;
    }
    
    m_pages.insert(page);
    m_lru_pages.insert(m_lru_pages.begin(), page);
}

void VTPageLRU::RemovePage(const VTPage& page)
{
    if (!m_pages.contains(page))
    {
        return;
    }

    m_pages.erase(page);
    m_lru_pages.remove(page);
}

void VTPageLRU::TouchPage(const VTPage& page)
{
    RemovePage(page);
    AddPage(page);
}

const VTPage& VTPageLRU::GetPageForFree() const
{
    return m_lru_pages.back();
}

VTPhysicalTexture::VTPhysicalTexture(int texture_size, int page_size, int border)
    : m_texture_size(texture_size), m_page_size(page_size), m_border(border)
{
    m_page_table_size = m_texture_size / (m_page_size + 2 * m_border);
    
    for (int x = 0; x < m_page_table_size; x++)
    {
        for (int y = 0; y < m_page_table_size; y++)
        {
            m_available_pages.emplace_back(x, y);
        }
    }

    m_physical_texture_data = std::make_shared<VTTextureData>(m_texture_size, m_texture_size, RHIDataFormat::R8G8B8A8_UNORM);
}

void VTPhysicalTexture::ProcessRequestResult(const std::vector<VTPageData>& results)
{
    for (const auto& result : results)
    {
        if (m_page_allocations.contains(result.page.PageHash()))
        {
            continue;
        }

        if (m_available_pages.empty())
        {
            const auto& page_to_free = m_page_lru_cache.GetPageForFree();
            m_available_pages.emplace_back(page_to_free.X, page_to_free.Y);
            m_page_allocations.erase(result.page.PageHash());
            m_page_lru_cache.RemovePage(page_to_free);
        }

        auto& page_allocation = m_page_allocations[result.page.PageHash()];
        bool found = GetAvailablePagesAndErase(page_allocation.X, page_allocation.Y);
        GLTF_CHECK(found && result.loaded);
        page_allocation.page = result.page;
        page_allocation.page_data = result.data;
    }
}

void VTPhysicalTexture::UpdateTextureData()
{
    m_physical_texture_data->ResetTextureData();
    for (const auto& page_allocation : m_page_allocations)
    {
        unsigned page_offset_x = page_allocation.second.X * (VirtualTextureSystem::VT_PAGE_SIZE + 2 * m_border) + m_border;
        unsigned page_offset_y = page_allocation.second.Y * (VirtualTextureSystem::VT_PAGE_SIZE + 2 * m_border) + m_border;
        m_physical_texture_data->UpdateRegionDataWithPixelData(page_offset_x, page_offset_y, VirtualTextureSystem::VT_PAGE_SIZE, VirtualTextureSystem::VT_PAGE_SIZE, page_allocation.second.page_data.get());
    }
}

bool VTPhysicalTexture::InitRenderResource(glTFRenderResourceManager& resource_manager)
{
    if (m_render_resource_init)
    {
        return true;
    }

    m_render_resource_init = true;
    RHITextureDesc page_table_texture_desc
    (
        "Physical Texture",
        m_texture_size,
        m_texture_size,
        RHIDataFormat::R8G8B8A8_UNORM,
        static_cast<RHIResourceUsageFlags>(RUF_ALLOW_SRV | RUF_TRANSFER_DST),
        {
            RHIDataFormat::R8G8B8A8_UNORM,
            glm::vec4{0,0,0,0}
        }
    );
    
    const bool allocated = resource_manager.GetMemoryManager().AllocateTextureMemory(resource_manager.GetDevice(), resource_manager, page_table_texture_desc, m_physical_texture);
    GLTF_CHECK(allocated);
    
    return true;
}

void VTPhysicalTexture::UpdateRenderResource(glTFRenderResourceManager& resource_manager)
{
    if (!m_physical_texture)
    {
        InitRenderResource(resource_manager);
    }

    const RHITextureUploadInfo upload_info
    {
        m_physical_texture_data->GetData(),
        m_physical_texture_data->GetDataSize(),
    };
    
    RHIUtils::Instance().UploadTextureData(resource_manager.GetCommandListForRecord(), resource_manager.GetMemoryManager(), resource_manager.GetDevice(), *m_physical_texture->m_texture, upload_info );
}

const std::map<VTPage::HashType, VTPhysicalPageAllocationInfo>& VTPhysicalTexture::GetPageAllocationInfos() const
{
    return m_page_allocations;
}

std::shared_ptr<IRHITextureAllocation> VTPhysicalTexture::GetTextureAllocation() const
{
    return m_physical_texture;
}

bool VTPhysicalTexture::GetAvailablePagesAndErase(int& x, int& y)
{
    if (m_available_pages.empty())
    {
        return false;
    }

    const auto last_page = m_available_pages[m_available_pages.size() - 1];
    x = last_page.first;
    y = last_page.second;
    m_available_pages.resize(m_available_pages.size() - 1);

    return true;
}
