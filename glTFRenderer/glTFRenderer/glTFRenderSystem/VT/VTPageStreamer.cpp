#include "VTPageStreamer.h"

VTPageStreamer::VTPageStreamer()
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
    m_pending_requests.push(page);
}

void VTPageStreamer::Tick()
{
    while (!m_pending_requests.empty())
    {
        auto page = m_pending_requests.front();
        m_pending_requests.pop();

        // Page streaming
        VTPageData page_data;
        page_data.page = page;
        
        if (m_logical_textures.contains(page.tex))
        {
            VTPageData result; 
            if (m_logical_textures[page.tex]->GetPageData(page, result))
            {
                m_request_results.push_back(result);    
            }
        }
        /*
        else if (m_data_accessor->TryGetPageData(page, page_data))
        {
            m_request_results.push_back(page_data);
        }
        // TODO: invalid page request
        */
    }

    //m_data_accessor->Tick();
}

std::vector<VTPageData> VTPageStreamer::GetRequestResultsAndClean()
{
    auto result = m_request_results;
    m_request_results.clear();
    return result;
}
