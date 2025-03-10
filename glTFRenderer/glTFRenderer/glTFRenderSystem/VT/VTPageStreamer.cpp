#include "VTPageStreamer.h"

VTPageStreamer::VTPageStreamer()
{
    m_data_accessor = std::make_shared<VTPageDataAccessor>();
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
        if (m_data_accessor->TryGetPageData(page, page_data))
        {
            m_request_results.push_back(page_data);
        }
    }
}

std::vector<VTPageData> VTPageStreamer::GetRequestResultsAndClean()
{
    auto result = m_request_results;
    m_request_results.clear();
    return result;
}
