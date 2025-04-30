#pragma once
#include "VTPageDataAccessor.h"
#include "VTPageTable.h"
#include "VTTextureTypes.h"

class SVTPageStreamer
{
public:
    SVTPageStreamer(unsigned page_process_count_per_frame);
    DECLARE_NON_COPYABLE_AND_VDTOR(SVTPageStreamer)

    void AddLogicalTexture(std::shared_ptr<VTLogicalTexture> logical_texture);
    
    void AddSVTPageRequest(const VTPage& page);
    void Tick();
    std::vector<VTPageData> GetRequestResultsAndClean();
    
protected:
    unsigned m_page_process_count_per_frame;
    
    std::shared_ptr<VTPageDataAccessor> m_data_accessor;
    
    std::queue<VTPage> m_pending_requests;
    std::set<VTPage::HashType> m_requested_page_hash;
    
    std::vector<VTPageData> m_request_results;

    std::map<unsigned, std::shared_ptr<VTLogicalTexture>> m_logical_textures;
};
