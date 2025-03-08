#pragma once
#include "VTPageDataAccessor.h"
#include "VTPageTable.h"

class VTPageStreamer
{
public:
    void AddPageRequest(const VTPage& page);
    void Tick();
    const std::vector<VTPageData>& GetRequestResultsAndClean();
    
protected:    
    std::shared_ptr<VTPageDataAccessor> m_data_accessor;
    std::queue<VTPage> m_pending_requests;
    std::vector<VTPageData> m_request_results;
};
