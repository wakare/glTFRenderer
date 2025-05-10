#include "SVTPageStreamer.h"

SVTPageStreamer::SVTPageStreamer(unsigned page_process_count_per_frame)
    : m_page_process_count_per_frame(page_process_count_per_frame)
{
    m_data_accessor = std::make_shared<VTPageDataAccessor>();
}

void SVTPageStreamer::AddLogicalTexture(std::shared_ptr<VTLogicalTextureBase> logical_texture)
{
    if (m_logical_textures.contains(logical_texture->GetTextureId()))
    {
        return;
    }

    m_logical_textures[logical_texture->GetTextureId()] = logical_texture;
}

void SVTPageStreamer::AddSVTPageRequest(const VTPage& page)
{
    if (page.type == VTPageType::SVT_PAGE && !m_pending_requested_pages.contains(page.PageHash()))
    {
        m_pending_requested_pages.try_emplace(page.PageHash(), page);
    }
}

void SVTPageStreamer::Tick()
{
    unsigned processed_page_count = 0;
    while (!m_pending_requested_pages.empty() && processed_page_count < m_page_process_count_per_frame)
    {
        auto page_iter = m_pending_requested_pages.begin();

        // Page streaming
        VTPageData page_data;
        page_data.page = page_iter->second;
        
        if (m_logical_textures.contains(page_data.page.texture_id))
        {
            VTPageData result; 
            if (m_logical_textures[page_data.page.texture_id]->GetPageData(page_data.page, result))
            {
                m_request_results.push_back(result);    
            }
        }

        processed_page_count++;
        
        m_pending_requested_pages.erase(page_iter);
    }

    //m_data_accessor->Tick();
}

std::vector<VTPageData> SVTPageStreamer::GetRequestResultsAndClean()
{
    auto result = m_request_results;
    m_request_results.clear();
    return result;
}
