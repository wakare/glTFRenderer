#include "VirtualTextureSystem.h"

bool VirtualTextureSystem::InitRenderSystem()
{
    
    return true;
}

void VirtualTextureSystem::ShutdownRenderSystem()
{
    
}

void VirtualTextureSystem::TickRenderSystem()
{
    DrawFeedBackPass();

    std::vector<VTPage> pages;
    GatherPageRequest(pages);

    for (const auto& page : pages)
    {
        m_page_streamer->AddPageRequest(page);    
    }
    m_page_streamer->Tick();
    auto page_data = m_page_streamer->GetRequestResultsAndClean();

    m_physical_texture->ProcessRequestResult(page_data);

    // Update all page table based physical texture allocation info
    const auto& page_allocations = m_physical_texture->GetPageAllocationInfos();

    for (auto& logical_texture_info : m_logical_texture_infos)
    {
        auto& physical_texture = logical_texture_info.second.first;
        auto& page_table  = logical_texture_info.second.second;

        for (const auto& page_allocation : page_allocations)
        {
            if (page_allocation.second.page.tex == page_table->GetTextureId())
            {
                page_table->UpdatePage(page_allocation.second.page);
            }
        }
    }

    // Upload page table to texture resource
}

bool VirtualTextureSystem::RegisterTexture(std::shared_ptr<VTLogicalTexture> texture)
{
    std::shared_ptr<VTPageTable> page_table = std::make_shared<VTPageTable>();
    const int page_table_size = texture->GetSize() / VT_PAGE_SIZE;
    page_table->InitVTPageTable(texture->GetTextureId(), page_table_size, VT_PAGE_SIZE);
    m_logical_texture_infos[texture->GetTextureId()] = std::make_pair(texture, page_table);
    
    return true;
}

void VirtualTextureSystem::DrawFeedBackPass()
{
}

void VirtualTextureSystem::GatherPageRequest(std::vector<VTPage>& out_pages)
{
}
