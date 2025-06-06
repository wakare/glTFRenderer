#include "VirtualTextureSystem.h"

#include "glTFRenderPass/glTFRenderPassManager.h"
#include "glTFRenderPass/glTFComputePass/glTFComputePassVTFetchCS.h"

bool VirtualTextureSystem::InitRenderSystem(glTFRenderResourceManager& resource_manager)
{
    m_svt_page_streamer = std::make_shared<SVTPageStreamer>(VT_PAGE_PROCESS_COUNT_PER_FRAME);
    
    m_physical_textures.push_back(std::make_shared<VTPhysicalTexture>(VT_PHYSICAL_TEXTURE_SIZE, VT_PAGE_SIZE, VT_PHYSICAL_TEXTURE_BORDER, true));
    m_physical_textures.push_back(std::make_shared<VTPhysicalTexture>(VT_PHYSICAL_TEXTURE_SIZE, VT_PAGE_SIZE, VT_PHYSICAL_TEXTURE_BORDER, false));
    for (auto& texture : m_physical_textures)
    {
        texture->UpdateRenderResource(resource_manager);
    }
    
    return true;
}

void VirtualTextureSystem::SetupPass(glTFRenderResourceManager& resource_manager, glTFRenderPassManager& pass_manager, const glTFSceneGraph& scene_graph)
{
    for (const auto& logic_texture : m_logical_textures)
    {
        logic_texture.second->InitRenderResource(resource_manager);
        logic_texture.second->SetupPassManager(pass_manager);
        logic_texture.second->UpdateRenderResource(resource_manager);
    }

    for (const auto& feedback_id : GetVTFeedbackTextureRenderTargetIds())
    {
        std::shared_ptr<glTFComputePassVTFetchCS> fetch_pass = std::make_shared<glTFComputePassVTFetchCS>(feedback_id);
        pass_manager.AddRenderPass(PRE_SCENE, fetch_pass);
        m_fetch_passes.push_back(fetch_pass);
    }
}

void VirtualTextureSystem::ShutdownRenderSystem()
{
    
}

void VirtualTextureSystem::TickRenderSystem(glTFRenderResourceManager& resource_manager)
{
    UpdateRenderResource(resource_manager);
    
    std::vector<VTPage> svt_pages;
    std::vector<VTPage> rvt_pages;
    GatherPageRequest(svt_pages, rvt_pages);

    // Check page request count less than physical texture page capacity
    const size_t max_svt_page_count = GetSVTPhysicalTexture()->GetPageCapacity();
    const size_t max_rvt_page_count = GetRVTPhysicalTexture()->GetPageCapacity();
    if (svt_pages.size() > max_svt_page_count || rvt_pages.size() > max_rvt_page_count)
    {
        LOG_FORMAT_FLUSH("[WARN] Current frame need many pages[%lld] which larger than physical texture max capacity[%lld]\n", svt_pages.size(), max_svt_page_count)
    }

    for (const auto& page : svt_pages)
    {
        switch (page.type) {
        case VTPageType::SVT_PAGE:
            m_svt_page_streamer->AddSVTPageRequest(page);
            break;
        case VTPageType::RVT_PAGE:
            break;
        }
    }
    
    m_svt_page_streamer->Tick();
    auto svt_page_data = m_svt_page_streamer->GetRequestResultsAndClean();
    if (!svt_page_data.empty())
    {
        // SVT processing
        auto svt_physical_texture = GetSVTPhysicalTexture();
        svt_physical_texture->InsertPage(svt_page_data);
    }
    if (!rvt_pages.empty())
    {
        std::vector<VTPageData> rvt_page_data;
        rvt_page_data.reserve(rvt_pages.size());
        for (const auto& page : rvt_pages)
        {
            VTPageData page_data;
            page_data.page = page;
            rvt_page_data.push_back(page_data);
        }
        
        // RVT processing
        auto rvt_physical_texture = GetRVTPhysicalTexture();
        rvt_physical_texture->InsertPage(rvt_page_data);
    }

    bool has_pending_request = false;
    for (auto& physical_texture: m_physical_textures)
    {
        physical_texture->UpdateRenderResource(resource_manager);
        
        const auto& page_allocations = physical_texture->GetPageAllocations();
        for (auto& logical_texture_info : m_logical_textures)
        {
            auto& logical_texture = logical_texture_info.second;
            auto& page_table = logical_texture_info.second->GetPageTable();
            page_table.Invalidate();
        
            for (const auto& page_allocation : page_allocations)
            {
                if (page_allocation.second.page.texture_id == page_table.GetLogicalTextureId())
                {
                    page_table.TouchPageAllocation(page_allocation.second);
                }
            }
                
            page_table.UpdateTextureData();
            logical_texture->UpdateRenderResource(resource_manager);

            has_pending_request |= physical_texture->HasPendingStreamingPages();
        }
    }

    for (auto& fetch_pass : m_fetch_passes)
    {
        fetch_pass->SetRenderingEnabled(!has_pending_request);
    }
}

bool VirtualTextureSystem::HasTexture(std::shared_ptr<VTLogicalTextureBase> texture) const
{
    return m_logical_textures.contains(texture->GetTextureId());
}

bool VirtualTextureSystem::RegisterTexture(std::shared_ptr<VTLogicalTextureBase> texture)
{
    m_logical_textures[texture->GetTextureId()] = texture;
    m_svt_page_streamer->AddLogicalTexture(texture);

    switch (texture->GetLogicalTextureType()) {
    case LogicalTextureType::SVT:
        GetSVTPhysicalTexture()->RegisterLogicalTexture(texture);
        break;
    case LogicalTextureType::RVT:
        GetRVTPhysicalTexture()->RegisterLogicalTexture(texture);
        break;
    }
    
    return true;
}

bool VirtualTextureSystem::UpdateRenderResource(glTFRenderResourceManager& resource_manager)
{
    return true;
}

const std::map<int, std::shared_ptr<VTLogicalTextureBase>>& VirtualTextureSystem::
GetLogicalTextureInfos() const
{
    return m_logical_textures;
}

VTLogicalTextureBase& VirtualTextureSystem::
GetLogicalTextureInfo(unsigned virtual_texture_id) const
{
    return *m_logical_textures.at(virtual_texture_id);
}

std::shared_ptr<VTPhysicalTexture> VirtualTextureSystem::GetSVTPhysicalTexture() const
{
    for (const auto& physical_texture : m_physical_textures)
    {
        if (physical_texture->IsSVT())
        {
            return physical_texture;
        }
    }
    return nullptr;
}

std::shared_ptr<VTPhysicalTexture> VirtualTextureSystem::GetRVTPhysicalTexture() const
{
    for (const auto& physical_texture : m_physical_textures)
    {
        if (!physical_texture->IsSVT())
        {
            return physical_texture;
        }
    }
    return nullptr;
}

std::pair<unsigned, unsigned> VirtualTextureSystem::GetVTFeedbackTextureSize(unsigned width, unsigned height)
{
    return
    {
        width / VT_FEEDBACK_TEXTURE_SCALE_SIZE,
        height / VT_FEEDBACK_TEXTURE_SCALE_SIZE,
    };
}

unsigned VirtualTextureSystem::GetNextValidVTIdAndInc()
{
    return m_next_valid_vt_id++;
}

void VirtualTextureSystem::RegisterVTFeedbackTextureRenderTargetId(RenderPassResourceTableId id)
{
    m_feedback_render_target_ids.insert(id);
}

const std::set<RenderPassResourceTableId>& VirtualTextureSystem::GetVTFeedbackTextureRenderTargetIds() const
{
    return m_feedback_render_target_ids;
}

VTPageType VirtualTextureSystem::GetPageType(unsigned virtual_texture_id) const
{
    GLTF_CHECK(m_logical_textures.contains(virtual_texture_id));
    switch (m_logical_textures.at(virtual_texture_id)->GetLogicalTextureType()) {
    case LogicalTextureType::SVT:
        return VTPageType::SVT_PAGE;
        
    case LogicalTextureType::RVT:
        return VTPageType::RVT_PAGE;
    }

    return VTPageType::UNKNOWN;
}

void VirtualTextureSystem::GatherPageRequest(std::vector<VTPage>& out_svt_pages, std::vector<VTPage>& out_rvt_pages)
{
    std::set<VTPage> pages;
    for (const auto& fetch_pass  : m_fetch_passes)
    {
        if (!fetch_pass->IsFeedBackDataValid())
        {
            continue;
        }
        
        const auto& feedback_data = fetch_pass->GetFeedbackOutputData();
        for (const auto& feedback : feedback_data)
        {
            if (feedback.data[3] == 0)
            {
                continue;
            }
        
            VTPage page;
            page.X = feedback.data[0];
            page.Y = feedback.data[1];
            page.mip = feedback.data[2];
            page.texture_id = feedback.data[3];
            page.type = GetPageType(page.texture_id);

            if (!pages.contains(page))
            {
                pages.insert(page);
                if (page.type == VTPageType::SVT_PAGE)
                {
                    out_svt_pages.push_back(page);
                }
                else
                {
                    out_rvt_pages.push_back(page);
                }
            }
        }
        
        fetch_pass->SetFeedBackDataValid(false);
    }
}
