#include "VTTextureTypes.h"

#include <utility>

#include "VirtualTextureSystem.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIInterface/IRHISwapChain.h"
#include "SceneFileLoader/glTFImageIOUtil.h"

bool VTLogicalTexture::InitLogicalTexture(const RHITextureDesc& desc)
{
    GLTF_CHECK(desc.GetTextureWidth() > 0 && desc.GetTextureHeight() > 0 && desc.GetTextureWidth() == desc.GetTextureHeight());

    // TODO: reserve id 0 as invalid
    static unsigned _inner_texture_id = 1;
    m_texture_id = _inner_texture_id++;
    m_logical_texture_width = desc.GetTextureWidth();
    m_logical_texture_height = desc.GetTextureHeight();

    GLTF_CHECK(m_logical_texture_width == m_logical_texture_height);
    
    m_texture_data = desc.GetTextureData();
    m_texture_format = desc.GetDataFormat();
    m_texture_data_size = desc.GetTextureDataSize();

    return GeneratePageData();
}

bool VTLogicalTexture::InitRenderResource(glTFRenderResourceManager& resource_manager)
{
    if (m_render_resource_init)
    {
        return true;
    }

    m_render_resource_init = true;

    return true;
}

void VTLogicalTexture::UpdateRenderResource(glTFRenderResourceManager& resource_manager)
{
    InitRenderResource(resource_manager);
}

int VTLogicalTexture::GetTextureId() const
{
    return m_texture_id;
}

int VTLogicalTexture::GetSize() const
{
    return m_logical_texture_width;
}

bool VTLogicalTexture::GetPageData(const VTPage& page, VTPageData& out) const
{
    if (!m_page_data.contains(page.PageHash()))
    {
        return false;
    }

    out = m_page_data.at(page.PageHash());
    return true;
}

bool VTLogicalTexture::GeneratePageData()
{
    // mip 0
    VTPage::OffsetType PageX = m_logical_texture_width / VirtualTextureSystem::VT_PAGE_SIZE;
    VTPage::OffsetType PageY = m_logical_texture_height / VirtualTextureSystem::VT_PAGE_SIZE;
    GLTF_CHECK(PageX * VirtualTextureSystem::VT_PAGE_SIZE == m_logical_texture_width &&
        PageY * VirtualTextureSystem::VT_PAGE_SIZE == m_logical_texture_height);
    int mip = 0;

    const size_t channel = 4;
    const size_t page_size = VirtualTextureSystem::VT_PAGE_SIZE * VirtualTextureSystem::VT_PAGE_SIZE * channel;

    GLTF_CHECK(GetBytePerPixelByFormat(m_texture_format) == channel);
    
    for (VTPage::OffsetType X = 0; X < PageX; ++X)
    {
        for (VTPage::OffsetType Y = 0; Y < PageY; ++Y)
        {
            VTPageData page_data;
            page_data.page.X = X;
            page_data.page.Y = Y;
            page_data.page.mip = mip;
            page_data.page.tex = GetTextureId();

            page_data.loaded = true;
            page_data.data = std::make_shared<unsigned char[]>(page_size);
            memset(page_data.data.get(), 0, page_size);
            for (int px = 0; px < VirtualTextureSystem::VT_PAGE_SIZE; ++px)
            {
                for (int py = 0; py < VirtualTextureSystem::VT_PAGE_SIZE; ++py)
                {
                    // fill rgba
                    for (size_t channel_index = 0; channel_index < channel; ++channel_index)
                    {
                        page_data.data[(py * VirtualTextureSystem::VT_PAGE_SIZE + px) * channel + channel_index] =
                            m_texture_data[((Y * VirtualTextureSystem::VT_PAGE_SIZE + py)  * m_logical_texture_width + (X * VirtualTextureSystem::VT_PAGE_SIZE + px)) * channel + channel_index];    
                    }
                }
            }

            m_page_data[page_data.page.PageHash()] = page_data;
        }
    }

    PageX = PageX >> 1;
    PageY = PageY >> 1;
    ++mip;
    
    // mip >= 1
    while (PageX && PageY)
    {
        for (VTPage::OffsetType X = 0; X < PageX; ++X)
        {
            for (VTPage::OffsetType Y = 0; Y < PageY; ++Y)
            {
                VTPageData page_data;
                page_data.page.X = X;
                page_data.page.Y = Y;
                page_data.page.mip = mip;
                page_data.page.tex = m_texture_id;
                page_data.loaded = true;
                page_data.data = std::make_shared<unsigned char[]>(page_size);
                memset(page_data.data.get(), 0, page_size);

                // Get prior mip page data
                VTPage::OffsetType src_X = 2 * X;
                VTPage::OffsetType src_Y = 2 * Y;
                VTPage::OffsetType src_XPlusOne = static_cast<VTPage::OffsetType>(src_X + 1);
                VTPage::OffsetType src_YPlusOne = static_cast<VTPage::OffsetType>(src_Y + 1);

                VTPage source_pages[4] =
                    {
                        {.X= src_X,         .Y= src_Y,          .mip= static_cast<VTPage::MipType>(mip - 1), .tex = GetTextureId()},
                        {.X= src_XPlusOne,  .Y= src_Y,          .mip= static_cast<VTPage::MipType>(mip - 1), .tex = GetTextureId()},
                        {.X= src_XPlusOne,  .Y= src_YPlusOne,   .mip= static_cast<VTPage::MipType>(mip - 1), .tex = GetTextureId()},
                        {.X= src_X,         .Y= src_YPlusOne,   .mip= static_cast<VTPage::MipType>(mip - 1), .tex = GetTextureId()}
                    };

                for (const VTPage& source_page : source_pages)
                {
                    VTPageData source_page_data; 
                    const bool get = GetPageData(source_page, source_page_data);
                    GLTF_CHECK(get == true);
                    
                    size_t offset_x = (source_page.X - src_X) * VirtualTextureSystem::VT_PAGE_SIZE;
                    size_t offset_y = (source_page.Y - src_Y) * VirtualTextureSystem::VT_PAGE_SIZE;
                    
                    for (size_t px = 0; px < VirtualTextureSystem::VT_PAGE_SIZE; ++px)
                    {
                        for (size_t py = 0; py < VirtualTextureSystem::VT_PAGE_SIZE; ++py)
                        {
                            size_t dst_x = (offset_x + px) / 2;
                            size_t dst_y = (offset_y + py) / 2;
                            for (size_t channel_index = 0; channel_index < channel; ++channel_index)
                            {
                                page_data.data[channel * ((dst_y * VirtualTextureSystem::VT_PAGE_SIZE) + dst_x) + channel_index] +=
                                    (source_page_data.data[channel * ((py * VirtualTextureSystem::VT_PAGE_SIZE) + px) + channel_index] / 4);    
                            }
                        }
                    }
                }

                m_page_data[page_data.page.PageHash()] = page_data;
            }
        }
        
        PageX = PageX >> 1;
        PageY = PageY >> 1;
        ++mip;
    }

    //DumpGeneratedPageDataToFile();
    
    return true;
}

void VTLogicalTexture::DumpGeneratedPageDataToFile() const
{    
    for (const auto& page_data : m_page_data)
    {
        std::string output_file_name = std::format("DumpVTImageDir\\{0}.png", page_data.second.page.ToString());
        bool saved = glTFImageIOUtil::Instance().SaveAsPNG(output_file_name, page_data.second.data.get(), VirtualTextureSystem::VT_PAGE_SIZE, VirtualTextureSystem::VT_PAGE_SIZE);
        GLTF_CHECK(saved);
    }
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

VTPhysicalTexture::VTPhysicalTexture(int texture_size, int page_size, int border)
    : m_texture_size(texture_size), m_page_size(page_size), m_border(border)
{
    m_page_table_size = m_texture_size / (m_page_size + 2 * m_border);
    
    for (int x = 0; x < m_page_table_size; x++)
    {
        for (int y = 0; y < m_page_table_size; y++)
        {
            m_available_pages.emplace_back(x, y);
        }
    }

    m_physical_texture_data = std::make_shared<VTTextureData>(m_texture_size, m_texture_size, RHIDataFormat::R8G8B8A8_UNORM);
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
            GLTF_CHECK(m_page_allocations.contains(page_to_free.PageHash()));
            
            const auto& reuse_physical_page = m_page_allocations[page_to_free.PageHash()];
            GLTF_CHECK(0 <= reuse_physical_page.X && reuse_physical_page.X < m_page_table_size &&
                0 <= reuse_physical_page.Y && reuse_physical_page.Y < m_page_table_size);
            
            m_available_pages.emplace_back(reuse_physical_page.X, reuse_physical_page.Y);
            
            m_page_allocations.erase(page_to_free.PageHash());
            m_page_lru_cache.RemovePage(page_to_free);
        }

        auto& page_allocation = m_page_allocations[result.page.PageHash()];
        bool found = GetAvailablePagesAndErase(page_allocation.X, page_allocation.Y);
        GLTF_CHECK(found && result.loaded);
        page_allocation.page = result.page;
        page_allocation.page_data = result.data;
        m_page_lru_cache.AddPage(result.page);
    }
}

void VTPhysicalTexture::UpdateTextureData()
{
    m_physical_texture_data->ResetTextureData();
    for (const auto& page_allocation : m_page_allocations)
    {
        unsigned page_offset_x = page_allocation.second.X * (VirtualTextureSystem::VT_PAGE_SIZE + 2 * m_border) + m_border;
        unsigned page_offset_y = page_allocation.second.Y * (VirtualTextureSystem::VT_PAGE_SIZE + 2 * m_border) + m_border;
        m_physical_texture_data->UpdateRegionData(page_offset_x, page_offset_y, VirtualTextureSystem::VT_PAGE_SIZE, VirtualTextureSystem::VT_PAGE_SIZE, page_allocation.second.page_data.get());
    }
}

bool VTPhysicalTexture::InitRenderResource(glTFRenderResourceManager& resource_manager)
{
    if (m_render_resource_init)
    {
        return true;
    }

    m_render_resource_init = true;
    RHITextureDesc page_table_texture_desc
    (
        "Physical Texture",
        m_texture_size,
        m_texture_size,
        RHIDataFormat::R8G8B8A8_UNORM,
        static_cast<RHIResourceUsageFlags>(RUF_ALLOW_SRV | RUF_TRANSFER_DST),
        {
            RHIDataFormat::R8G8B8A8_UNORM,
            glm::vec4{0,0,0,0}
        }
    );
    
    const bool allocated = resource_manager.GetMemoryManager().AllocateTextureMemory(resource_manager.GetDevice(), resource_manager, page_table_texture_desc, m_physical_texture);
    GLTF_CHECK(allocated);
    
    return true;
}

void VTPhysicalTexture::UpdateRenderResource(glTFRenderResourceManager& resource_manager)
{
    if (!m_physical_texture)
    {
        InitRenderResource(resource_manager);
    }

    const RHITextureMipUploadInfo upload_info
    {
        m_physical_texture_data->GetData(),
        m_physical_texture_data->GetDataSize(),
        0
    };
    
    RHIUtils::Instance().UploadTextureData(resource_manager.GetCommandListForRecord(), resource_manager.GetMemoryManager(), resource_manager.GetDevice(), *m_physical_texture->m_texture, upload_info );
    resource_manager.CloseCurrentCommandListAndExecute({},false);
}

const std::map<VTPage::HashType, VTPhysicalPageAllocationInfo>& VTPhysicalTexture::GetPageAllocationInfos() const
{
    return m_page_allocations;
}

std::shared_ptr<IRHITextureAllocation> VTPhysicalTexture::GetTextureAllocation() const
{
    return m_physical_texture;
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
    GLTF_CHECK(0 <= x && x < m_page_table_size && 0 <= y && y < m_page_table_size);
    
    m_available_pages.resize(m_available_pages.size() - 1);

    return true;
}
