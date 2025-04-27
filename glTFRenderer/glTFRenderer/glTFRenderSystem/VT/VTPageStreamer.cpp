#include "VTPageStreamer.h"

VTPageStreamer::VTPageStreamer(unsigned page_process_count_per_frame)
    : m_page_process_count_per_frame(page_process_count_per_frame)
{
    m_data_accessor = std::make_shared<VTPageDataAccessor>();
}

void VTPageStreamer::AddLogicalTexture(std::shared_ptr<VTLogicalTexture> logical_texture)
{
    if (m_logical_textures.contains(logical_texture->GetTextureId()))
    {
        return;
    }

    m_logical_textures[logical_texture->GetTextureId()] = logical_texture;
}

void VTPageStreamer::AddPageRequest(const VTPage& page)
{
    if (!m_requested_page_hash.contains(page.PageHash()))
    {
        m_requested_page_hash.insert(page.PageHash());
        m_pending_requests.push(page);    
    }
}

void VTPageStreamer::Tick()
{
    unsigned processed_page_count = 0;
    while (!m_pending_requests.empty() && processed_page_count < m_page_process_count_per_frame)
    {
        auto page = m_pending_requests.front();
        m_pending_requests.pop();
        m_requested_page_hash.erase(page.PageHash());

        // Page streaming
        VTPageData page_data;
        page_data.page = page;
        
        if (m_logical_textures.contains(page.logical_tex_id))
        {
            VTPageData result; 
            if (m_logical_textures[page.logical_tex_id]->GetPageData(page, result))
            {
                m_request_results.push_back(result);    
            }
        }

        processed_page_count++;
    }

    //m_data_accessor->Tick();
}

std::vector<VTPageData> VTPageStreamer::GetRequestResultsAndClean()
{
    auto result = m_request_results;
    m_request_results.clear();
    return result;
}
