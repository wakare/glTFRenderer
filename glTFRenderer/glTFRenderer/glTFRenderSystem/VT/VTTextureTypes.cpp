#include "VTTextureTypes.h"

#include <utility>

bool VTLogicalTexture::InitLogicalTexture(int size, int texture_id)
{
    GLTF_CHECK(size >= 0 && texture_id >= 0);
    
    m_texture_id = texture_id;
    m_size = size;
    
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

VTPhysicalTexture::VTPhysicalTexture(int size, int page_size, int border)
    : m_page_table_size(size), m_page_size(page_size), m_border(border)
{
    for (int x = 0; x < m_page_table_size; x++)
    {
        for (int y = 0; y < m_page_table_size; y++)
        {
            m_available_pages.emplace_back(x, y);
        }
    }
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

const std::map<VTPage::HashType, VTPhysicalPageAllocationInfo>& VTPhysicalTexture::GetPageAllocationInfos() const
{
    return m_page_allocations;
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