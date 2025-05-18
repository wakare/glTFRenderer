#pragma once
#include <memory>
#include <mutex>

#include "VTPageTable.h"
#include "AsyncFileLoader.h"
#include "RendererCommon.h"

struct VTPageData
{
    VTPage page;
    std::shared_ptr<unsigned char[]> data;
    size_t data_size;
    
    bool loaded{false};
};

// Util class for access page texture data
class VTPageDataAccessor
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VTPageDataAccessor)

    bool Tick();
    bool TryGetPageData(const VTPage& page, VTPageData& page_data);

    std::mutex m_page_data_mutex;
    std::map<VTPage::HashType, VTPageData> m_page_data;

    FileReadManager m_file_read_manager;
};
