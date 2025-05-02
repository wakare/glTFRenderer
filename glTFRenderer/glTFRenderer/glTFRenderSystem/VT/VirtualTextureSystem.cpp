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
        {
            switch (page.type) {
            case VTPageType::SVT_PAGE:
                m_svt_page_streamer->AddSVTPageRequest(page);
                break;
            case VTPageType::RVT_PAGE:
                break;
            }
        }
    }
    
    m_svt_page_streamer->Tick();
    auto svt_page_data = m_svt_page_streamer->GetRequestResultsAndClean();
    if (!svt_page_data.empty())
    {
        // SVT processing
        {
            auto svt_physical_texture = GetSVTPhysicalTexture();
            svt_physical_texture->InsertPage(svt_page_data);
        }
    }
    if (!rvt_pages.empty())
    {
        std::vector<VTPageData> rvt_page_data;
        rvt_page_data.resize(rvt_pages.size());
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
        }
    }
}

bool VirtualTextureSystem::HasTexture(std::shared_ptr<VTLogicalTexture> texture) const
{
    return m_logical_textures.contains(texture->GetTextureId());
}

bool VirtualTextureSystem::RegisterTexture(std::shared_ptr<VTLogicalTexture> texture)
{
    m_logical_textures[texture->GetTextureId()] = texture;
    m_svt_page_streamer->AddLogicalTexture(texture);

    if (texture->IsSVT())
    {
        GetSVTPhysicalTexture()->RegisterLogicalTexture(texture);
    }
    else if (texture->IsRVT())
    {
        GetRVTPhysicalTexture()->RegisterLogicalTexture(texture);
    }
    
    return true;
}

bool VirtualTextureSystem::UpdateRenderResource(glTFRenderResourceManager& resource_manager)
{
    return true;
}

const std::map<int, std::shared_ptr<VTLogicalTexture>>& VirtualTextureSystem::
GetLogicalTextureInfos() const
{
    return m_logical_textures;
}

VTLogicalTexture& VirtualTextureSystem::
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

VTPageType VirtualTextureSystem::GetPageType(unsigned virtual_texture_id) const
{
    GLTF_CHECK(m_logical_textures.contains(virtual_texture_id));
    return m_logical_textures.at(virtual_texture_id)->IsSVT() ? VTPageType::SVT_PAGE : VTPageType::RVT_PAGE;
}

void VirtualTextureSystem::GatherPageRequest(std::vector<VTPage>& out_svt_pages, std::vector<VTPage>& out_rvt_pages)
{
    std::set<VTPage> pages;
    for (const auto& logical_texture  : m_logical_textures)
    {
        std::vector<VTPage>& out_page_results = logical_texture.second->IsSVT() ? out_svt_pages : out_rvt_pages;
        
        auto& fetch_pass = dynamic_cast<glTFComputePassVTFetchCS&>(logical_texture.second->GetFetchPass());
        if (!fetch_pass.IsFeedBackDataValid())
        {
            continue;
        }
        
        const auto& feedback_data = fetch_pass.GetFeedbackOutputData();
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
                out_page_results.push_back(page);
            }
        }
        
        fetch_pass.SetFeedBackDataValid(false);
    }
}
