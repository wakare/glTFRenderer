#include "VTPageDataAccessor.h"

static std::string GetPageFilename(const VTPage& page)
{
    return std::to_string(page.PageHash());
}

bool VTPageDataAccessor::Tick()
{
    FileReadResult result;
    while (m_file_read_manager.getResult(result))
    {
        GLTF_CHECK(result.success);
        if (!result.success)
        {
            return false;
        }
        
        VTPage::HashType hash = std::stoull(result.filename);

        auto& page_data = m_page_data[hash];
        page_data.data = result.data;
        page_data.loaded = true;
    }

    return true;
}

bool VTPageDataAccessor::TryGetPageData(const VTPage& page, VTPageData& page_data)
{
    if (m_page_data.contains(page.PageHash()))
    {
        if (!m_page_data[page.PageHash()].loaded)
        {
            return false;
        }
        
        page_data = m_page_data[page.PageHash()];
        return true;
    }

    m_file_read_manager.addTask(GetPageFilename(page));
    m_page_data[page.PageHash()].page = page;
    
    return false;
}
