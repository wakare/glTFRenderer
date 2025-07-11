#include "VTTextureTypes.h"

#include <utility>

#include "VirtualTextureSystem.h"
#include "glTFRenderPass/glTFRenderPassManager.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRenderPass/glTFComputePass/glTFComputePassVTFetchCS.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassMeshRVTPageRendering.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassMeshVSMPageRendering.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassMeshVTFeedbackBase.h"
#include "RHIUtils.h"
#include "RHIInterface/IRHISwapChain.h"
#include "SceneFileLoader/glTFImageIOUtil.h"

bool VTLogicalTextureBase::InitLogicalTexture(const RHITextureDesc& desc, const VTLogicalTextureConfig& config)
{
    GLTF_CHECK(desc.GetTextureWidth() > 0 && desc.GetTextureHeight() > 0 && desc.GetTextureWidth() == desc.GetTextureHeight());

    m_texture_id = config.virtual_texture_id;
    
    m_logical_texture_width = desc.GetTextureWidth();
    m_logical_texture_height = desc.GetTextureHeight();

    GLTF_CHECK(m_logical_texture_width == m_logical_texture_height);
    
    m_texture_data = desc.GetTextureData();
    m_texture_format = desc.GetDataFormat();
    m_texture_data_size = desc.GetTextureDataSize();

    RETURN_IF_FALSE(GeneratePageData());

    m_page_table = std::make_shared<VTPageTable>();
    const int page_table_size = m_logical_texture_width / VirtualTextureSystem::VT_PAGE_SIZE;
    RETURN_IF_FALSE(m_page_table->InitVTPageTable(m_texture_id, page_table_size, VirtualTextureSystem::VT_PAGE_SIZE));

    return true;
}

bool VTLogicalTextureBase::InitRenderResource(glTFRenderResourceManager& resource_manager)
{
    if (m_render_resource_init)
    {
        return true;
    }

    m_page_table->InitRenderResource(resource_manager);
    
    m_render_resource_init = true;

    return true;
}

bool VTLogicalTextureBase::SetupPassManager(glTFRenderPassManager& pass_manager) const
{
    GLTF_CHECK(m_feedback_pass);
    
    pass_manager.AddRenderPass(PRE_SCENE, m_feedback_pass);
    
    return true;
}

void VTLogicalTextureBase::UpdateRenderResource(glTFRenderResourceManager& resource_manager)
{
    m_page_table->UpdateRenderResource(resource_manager);
}

int VTLogicalTextureBase::GetTextureId() const
{
    return m_texture_id;
}

int VTLogicalTextureBase::GetSize() const
{
    return m_logical_texture_width;
}

VTPageTable& VTLogicalTextureBase::GetPageTable() const
{
    return *m_page_table;
}

glTFRenderPassBase& VTLogicalTextureBase::GetFeedbackPass() const
{
    GLTF_CHECK(m_feedback_pass);
    return *m_feedback_pass;
}

void VTLogicalTextureBase::SetEnableGatherRequest(bool enable)
{
    m_gather_request_enabled = enable;
    m_feedback_pass->SetRenderingEnabled(m_gather_request_enabled);
}

void VTLogicalTextureBase::DumpGeneratedPageDataToFile() const
{    
    for (const auto& page_data : m_page_data)
    {
        std::string output_file_name = std::format("DumpVTImageDir\\{0}.png", page_data.second.page.ToString());
        bool saved = glTFImageIOUtil::Instance().SaveAsPNG(output_file_name, page_data.second.data.get(), VirtualTextureSystem::VT_PAGE_SIZE, VirtualTextureSystem::VT_PAGE_SIZE);
        GLTF_CHECK(saved);
    }
}

bool VTLogicalTextureSVT::InitRenderResource(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(VTLogicalTextureBase::InitRenderResource(resource_manager))
    
    m_feedback_pass = std::make_shared<glTFGraphicsPassMeshVTFeedbackSVT>();
    resource_manager.GetRenderSystem<VirtualTextureSystem>()->RegisterVTFeedbackTextureRenderTargetId(GetVTFeedBackId(m_feedback_pass->GetID()));
    
    return true;
}

bool VTLogicalTextureSVT::GetPageData(const VTPage& page, VTPageData& out) const
{
    if (!m_page_data.contains(page.PageHash()))
    {
        return false;
    }

    out = m_page_data.at(page.PageHash());
    return true;
}

bool VTLogicalTextureSVT::GeneratePageData()
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
            page_data.page.texture_id = GetTextureId();

            page_data.loaded = true;
            page_data.data = std::make_shared<unsigned char[]>(page_size);
            memset(page_data.data.get(), 0, page_size);
            page_data.data_size = page_size;
            
            for (int px = 0; px < VirtualTextureSystem::VT_PAGE_SIZE; ++px)
            {
                for (int py = 0; py < VirtualTextureSystem::VT_PAGE_SIZE; ++py)
                {
                    // fill rgba
                    for (size_t channel_index = 0; channel_index < channel; ++channel_index)
                    {
                        page_data.data[(py * VirtualTextureSystem::VT_PAGE_SIZE + px) * channel + channel_index] =
                            m_texture_data[((Y * VirtualTextureSystem::VT_PAGE_SIZE + py)  * m_logical_texture_width +
                                (X * VirtualTextureSystem::VT_PAGE_SIZE + px)) * channel + channel_index];    
                    }
                }
            }

            m_page_data[page_data.page.PageHash()] = page_data;
        }
    }

    PageX = PageX >> 1; PageY = PageY >> 1; ++mip;
    
    // mip >= 1
    while (PageX && PageY)
    {
        for (VTPage::OffsetType X = 0; X < PageX; ++X)
        {
            for (VTPage::OffsetType Y = 0; Y < PageY; ++Y)
            {
                VTPageData page_data;
                page_data.page.X = X; page_data.page.Y = Y;
                page_data.page.mip = mip;
                page_data.page.texture_id = m_texture_id;
                page_data.loaded = true;
                page_data.data = std::make_shared<unsigned char[]>(page_size);
                memset(page_data.data.get(), 0, page_size);
                page_data.data_size = page_size;

                // Get prior mip page data
                VTPage::OffsetType src_X = 2 * X; VTPage::OffsetType src_Y = 2 * Y;
                VTPage::OffsetType src_XPlusOne = static_cast<VTPage::OffsetType>(src_X + 1);
                VTPage::OffsetType src_YPlusOne = static_cast<VTPage::OffsetType>(src_Y + 1);

                VTPage source_pages[4] =
                    {
                        {.X= src_X,         .Y= src_Y,          .mip= static_cast<VTPage::MipType>(mip - 1), .texture_id = GetTextureId()},
                        {.X= src_XPlusOne,  .Y= src_Y,          .mip= static_cast<VTPage::MipType>(mip - 1), .texture_id = GetTextureId()},
                        {.X= src_XPlusOne,  .Y= src_YPlusOne,   .mip= static_cast<VTPage::MipType>(mip - 1), .texture_id = GetTextureId()},
                        {.X= src_X,         .Y= src_YPlusOne,   .mip= static_cast<VTPage::MipType>(mip - 1), .texture_id = GetTextureId()}
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
                            size_t dst_x = (offset_x + px) / 2; size_t dst_y = (offset_y + py) / 2;
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
        
        PageX = PageX >> 1; PageY = PageY >> 1; ++mip;
    }

    //DumpGeneratedPageDataToFile();
    
    return true;
}

bool VTLogicalTextureRVT::SetupPassManager(glTFRenderPassManager& pass_manager) const
{
    RETURN_IF_FALSE(VTLogicalTextureBase::SetupPassManager(pass_manager))
    
    GLTF_CHECK(GetRVTPass());
    pass_manager.AddRenderPass(PRE_SCENE, GetRVTPass());
    
    return true;
}

void VTLogicalTextureRVT::UpdateRenderResource(glTFRenderResourceManager& resource_manager)
{
    VTLogicalTextureBase::UpdateRenderResource(resource_manager);

    GLTF_CHECK(m_pending_page_rendering_requests.size() <= 1);
    if (!m_pending_page_rendering_requests.empty())
    {
        GetRVTPass()->SetupNextPageRenderingInfo(resource_manager, m_pending_page_rendering_requests.front());
        m_pending_page_rendering_requests.clear();
    }
}

bool VTLogicalTextureRVT::InitRenderResource(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(VTLogicalTextureBase::InitRenderResource(resource_manager))
    m_feedback_pass = std::make_shared<glTFGraphicsPassMeshVTFeedbackRVT>(m_texture_id);
    resource_manager.GetRenderSystem<VirtualTextureSystem>()->RegisterVTFeedbackTextureRenderTargetId(GetVTFeedBackId(m_feedback_pass->GetID()));
    
    return true;
}

bool VTLogicalTextureRVT::GetPageData(const VTPage& page, VTPageData& out) const
{
    out.page = page;
    out.loaded = true;
    out.data = nullptr;
    
    return true;
}

void VTLogicalTextureRVT::EnqueuePageRenderingRequest(const std::vector<RVTPageRenderingInfo>& requests)
{
    // TODO: Enable rendering more pages per frame
    GLTF_CHECK(requests.size() == 1);
    m_pending_page_rendering_requests = requests;
}

bool VTLogicalTextureRVT::GeneratePageData()
{
    return true;
}

VTShadowmapLogicalTexture::VTShadowmapLogicalTexture(unsigned light_id)
    : m_light_id(light_id)
{
}

bool VTShadowmapLogicalTexture::InitRenderResource(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(VTLogicalTextureRVT::InitRenderResource(resource_manager))

    VSMConfig vsm_config;
    vsm_config.virtual_texture_id = m_texture_id;
    vsm_config.m_shadowmap_config.light_id = m_light_id;
    vsm_config.m_shadowmap_config.shadowmap_width = m_logical_texture_width;
    vsm_config.m_shadowmap_config.shadowmap_height = m_logical_texture_height;
    
    m_vsm_rvt_pass = std::make_shared<glTFGraphicsPassMeshVSMPageRendering>(vsm_config); 
    
    return true;
}

unsigned VTShadowmapLogicalTexture::GetLightId() const
{
    return m_light_id;
}

std::shared_ptr<glTFGraphicsPassMeshRVTPageRendering> VTShadowmapLogicalTexture::GetRVTPass() const
{
    return m_vsm_rvt_pass;
}

void VTPageLRU::AddPage(const VTPage& page)
{
    if (m_pages.contains(page))
    {
        m_lru_pages.remove(page);
        m_lru_pages.insert(m_lru_pages.begin(), page);
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

VTPhysicalTexture::VTPhysicalTexture(int texture_size, int page_size, int border, bool svt)
    : m_texture_size(texture_size), m_page_size(page_size), m_border(border), m_svt(svt)
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

void VTPhysicalTexture::InsertPage(const std::vector<VTPageData>& pages_to_insert)
{
    std::set<VTPage::HashType> removed_page_hashes;

    for (const auto& page_to_insert : pages_to_insert)
    {
        if ((page_to_insert.page.type == VTPageType::SVT_PAGE && !m_svt) ||
            (page_to_insert.page.type == VTPageType::RVT_PAGE && m_svt))
        {
            continue;
        }

        if (!m_logical_textures.contains(page_to_insert.page.texture_id))
        {
            continue;
        }

        if (m_page_allocations.contains(page_to_insert.page.PageHash()))
        {
            m_page_lru_cache.AddPage(page_to_insert.page);
            continue;
        }

        if (m_available_pages.empty())
        {
            const auto& page_to_remove = m_page_lru_cache.GetPageForFree();
            GLTF_CHECK(m_page_allocations.contains(page_to_remove.PageHash()));
            
            const auto& reuse_physical_page = m_page_allocations[page_to_remove.PageHash()];
            GLTF_CHECK(0 <= reuse_physical_page.X && reuse_physical_page.X < m_page_table_size &&
                0 <= reuse_physical_page.Y && reuse_physical_page.Y < m_page_table_size);
            
            m_available_pages.emplace_back(reuse_physical_page.X, reuse_physical_page.Y);
            
            m_page_allocations.erase(page_to_remove.PageHash());
            m_pending_streaming_pages.erase(page_to_remove.PageHash());
            m_page_lru_cache.RemovePage(page_to_remove);

            removed_page_hashes.insert(page_to_remove.PageHash());
        }

        auto& page_allocation = m_page_allocations[page_to_insert.page.PageHash()];
        m_pending_streaming_pages.emplace(page_to_insert.page.PageHash());
        bool found = GetAvailablePagesAndErase(page_allocation.X, page_allocation.Y);
        GLTF_CHECK(found);
        page_allocation.page = page_to_insert.page;
        page_allocation.page_data = page_to_insert.data;
        page_allocation.page_size = page_to_insert.data_size;
        m_page_lru_cache.AddPage(page_to_insert.page);
    }

    // Check
    if (!removed_page_hashes.empty())
    {
        for (auto& page : pages_to_insert)
        {
            GLTF_CHECK(!removed_page_hashes.contains( page.page.PageHash()));
        }    
    }

    if (!m_svt)
    {
        for (const auto& allocation : m_page_allocations)
        {
            // VSM need update even these page has been allocated 
            m_pending_streaming_pages.emplace(allocation.second.page.PageHash());    
        }
    }
}

bool VTPhysicalTexture::InitRenderResource(glTFRenderResourceManager& resource_manager)
{
    if (m_render_resource_init)
    {
        return true;
    }

    m_render_resource_init = true;
    RHIResourceUsageFlags flags = static_cast<RHIResourceUsageFlags>(RUF_ALLOW_SRV | RUF_TRANSFER_DST);
    if (!m_svt)
    {
        flags = static_cast<RHIResourceUsageFlags>(flags | RUF_ALLOW_DEPTH_STENCIL);
    }

    RHIDataFormat physical_texture_format = m_svt ? RHIDataFormat::R8G8B8A8_UNORM : RHIDataFormat::R32_TYPELESS;
    RHITextureDesc physical_texture_desc
    (
        "Physical Texture",
        m_texture_size,
        m_texture_size,
        physical_texture_format,
        flags,
        {
            .clear_format{RHIDataFormat::D32_FLOAT},
            .clear_depth_stencil{1.0f, 0}
        }
    );
    
    const bool allocated = resource_manager.GetMemoryManager().AllocateTextureMemory(resource_manager.GetDevice(), physical_texture_desc, m_physical_texture);
    GLTF_CHECK(allocated);
    
    m_physical_texture_data->ResetTextureData();
    
    return true;
}

void VTPhysicalTexture::UpdateRenderResource(glTFRenderResourceManager& resource_manager)
{
    if (!m_physical_texture)
    {
        InitRenderResource(resource_manager);
    }

    if (m_svt)
    {
        for (const auto& allocation : m_page_allocations)
        {
            if (!m_pending_streaming_pages.contains(allocation.first))
            {
                continue;
            }
            m_pending_streaming_pages.erase(allocation.first);

            int page_offset_x = allocation.second.X * (VirtualTextureSystem::VT_PAGE_SIZE + 2 * m_border) + m_border;
            int page_offset_y = allocation.second.Y * (VirtualTextureSystem::VT_PAGE_SIZE + 2 * m_border) + m_border;
        
            const RHITextureMipUploadInfo upload_info
            {
                allocation.second.page_data,
                allocation.second.page_size,
                page_offset_x, page_offset_y,
                VirtualTextureSystem::VT_PAGE_SIZE,
                VirtualTextureSystem::VT_PAGE_SIZE,
                0
            };

            RHIUtilInstanceManager::Instance().UploadTextureData(resource_manager.GetCommandListForRecord(),
                resource_manager.GetMemoryManager(), resource_manager.GetDevice(), *m_physical_texture->m_texture, upload_info );
        }
    }
    else
    {
        if (!m_pending_streaming_pages.empty())
        {
            auto page_iter = m_pending_streaming_pages.begin();
            GLTF_CHECK(m_page_allocations.contains(*page_iter));
            const auto& page_allocation = m_page_allocations[*page_iter];
            
            auto logic_texture = m_logical_textures[page_allocation.page.texture_id];
            GLTF_CHECK(logic_texture && logic_texture->GetLogicalTextureType() == LogicalTextureType::RVT);

            RVTPageRenderingInfo page_rendering_info{};
            page_rendering_info.physical_page_size = m_page_size;
            auto page_with_border_length = (VirtualTextureSystem::VT_PAGE_SIZE + 2 * VirtualTextureSystem::VT_PHYSICAL_TEXTURE_BORDER);
            page_rendering_info.physical_page_x = page_allocation.X * page_with_border_length + VirtualTextureSystem::VT_PHYSICAL_TEXTURE_BORDER;
            page_rendering_info.physical_page_y = page_allocation.Y * page_with_border_length + VirtualTextureSystem::VT_PHYSICAL_TEXTURE_BORDER;

            auto mip_page_size = m_page_size << page_allocation.page.mip; 
            page_rendering_info.page_x = static_cast<float>(page_allocation.page.X * mip_page_size) / static_cast<float>(logic_texture->GetSize());

            page_rendering_info.page_y = static_cast<float>((page_allocation.page.Y) * mip_page_size) / static_cast<float>(logic_texture->GetSize());
            page_rendering_info.mip_page_size = static_cast<float>(mip_page_size) / static_cast<float>(logic_texture->GetSize());
            
            dynamic_cast<VTLogicalTextureRVT&>(*logic_texture).EnqueuePageRenderingRequest({page_rendering_info});

            m_pending_streaming_pages.erase(page_iter);    
        }
    }
    
    resource_manager.CloseCurrentCommandListAndExecute({},false);

    for (auto& logical_texture : m_logical_textures)
    {
        //logical_texture.second->SetEnableGatherRequest(!HasPendingStreamingPages());
        logical_texture.second->SetEnableGatherRequest(true);
    }
}

const std::map<VTPage::HashType, VTPhysicalPageAllocationInfo>& VTPhysicalTexture::GetPageAllocations() const
{
    return m_page_allocations;
}

std::shared_ptr<IRHITextureAllocation> VTPhysicalTexture::GetTextureAllocation() const
{
    return m_physical_texture;
}

bool VTPhysicalTexture::IsSVT() const
{
    return m_svt;
}

unsigned VTPhysicalTexture::GetPageCapacity() const
{
    return m_page_table_size * m_page_table_size;
}

void VTPhysicalTexture::RegisterLogicalTexture(std::shared_ptr<VTLogicalTextureBase> logical_texture)
{
    m_logical_textures.try_emplace(logical_texture->GetTextureId(), logical_texture);
}

bool VTPhysicalTexture::HasPendingStreamingPages() const
{
    return !m_pending_streaming_pages.empty();
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
