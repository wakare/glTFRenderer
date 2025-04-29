#include "VirtualTextureSystem.h"

#include "glTFRenderPass/glTFRenderPassManager.h"
#include "glTFRenderPass/glTFComputePass/glTFComputePassVTFetchCS.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassMeshVTFeedback.h"

bool VirtualTextureSystem::InitRenderSystem(glTFRenderResourceManager& resource_manager)
{
    m_page_streamer = std::make_shared<VTPageStreamer>(VT_PAGE_PROCESS_COUNT_PER_FRAME);
    
    m_physical_virtual_textures.push_back(std::make_shared<VTPhysicalTexture>(VT_PHYSICAL_TEXTURE_SIZE, VT_PAGE_SIZE, VT_PHYSICAL_TEXTURE_BORDER, true));
    m_physical_virtual_textures.push_back(std::make_shared<VTPhysicalTexture>(VT_PHYSICAL_TEXTURE_SIZE, VT_PAGE_SIZE, VT_PHYSICAL_TEXTURE_BORDER, false));
    for (auto& texture : m_physical_virtual_textures)
    {
        texture->UpdateRenderResource(resource_manager);
    }

    return true;
}

void VirtualTextureSystem::SetupPass(glTFRenderResourceManager& resource_manager, glTFRenderPassManager& pass_manager, const glTFSceneGraph& scene_graph)
{
    for (const auto& logic_texture : m_logical_texture_infos)
    {
        auto feedback_pass = std::make_shared<glTFGraphicsPassMeshVTFeedback>(logic_texture.first, !!dynamic_cast<VTShadowmapLogicalTexture*>(logic_texture.second.get()));
        pass_manager.AddRenderPass(POST_SCENE, feedback_pass);
        
        auto fetch_feedback_pass = std::make_shared<glTFComputePassVTFetchCS>(logic_texture.first);
        pass_manager.AddRenderPass(POST_SCENE, fetch_feedback_pass);

        m_logical_texture_feedback_passes[logic_texture.first] = {feedback_pass, fetch_feedback_pass};

        logic_texture.second->UpdateRenderResource(resource_manager);
    }
    
    InitFeedBackPass();
}

void VirtualTextureSystem::ShutdownRenderSystem()
{
    
}

void VirtualTextureSystem::TickRenderSystem(glTFRenderResourceManager& resource_manager)
{
    //resource_manager.CloseCurrentCommandListAndExecute({},true);
    
    UpdateRenderResource(resource_manager);
    
    std::vector<VTPage> pages;
    GatherPageRequest(pages);

    // Check page request count less than physical texture page capacity
    const size_t max_page_count = GetSVTPhysicalTexture()->GetPageCapacity();
    if (pages.size() > max_page_count)
    {
        LOG_FORMAT_FLUSH("[WARN] Current frame need many pages[%lld] which larger than physical texture max capacity[%lld]\n", pages.size(), max_page_count)
    }
    
    for (const auto& page : pages)
    {
        if (!m_loaded_page_hashes.contains(page.PageHash()))
        {
            m_page_streamer->AddPageRequest(page);
        }
    }
    
    m_page_streamer->Tick();
    
    auto page_data = m_page_streamer->GetRequestResultsAndClean();
    m_loaded_page_hashes.clear();
    for (auto& texture : m_physical_virtual_textures)
    {
        texture->InsertPage(page_data);
        texture->UpdateRenderResource(resource_manager);
        texture->ResetDirtyPages();
        
        const auto& page_allocations = texture->GetPageAllocationInfos();
        for (const auto& page_allocation : page_allocations)
        {
            if (!m_loaded_page_hashes.contains(page_allocation.second.page.PageHash()))
            {
                m_loaded_page_hashes.insert(page_allocation.second.page.PageHash());
            }
        }
        
        for (auto& logical_texture_info : m_logical_texture_infos)
        {
            auto& logical_texture = logical_texture_info.second;
            auto& page_table = logical_texture_info.second->GetPageTable();
            page_table.Invalidate();
        
            for (const auto& page_allocation : page_allocations)
            {
                if (page_allocation.second.page.logical_tex_id == page_table.GetLogicalTextureId())
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
    return m_logical_texture_infos.contains(texture->GetTextureId());
}

bool VirtualTextureSystem::RegisterTexture(std::shared_ptr<VTLogicalTexture> texture)
{
    m_logical_texture_infos[texture->GetTextureId()] = texture;
    m_page_streamer->AddLogicalTexture(texture);
    
    return true;
}

bool VirtualTextureSystem::UpdateRenderResource(glTFRenderResourceManager& resource_manager)
{
    return true;
}

const std::map<int, std::shared_ptr<VTLogicalTexture>>& VirtualTextureSystem::
GetLogicalTextureInfos() const
{
    return m_logical_texture_infos;
}

VTLogicalTexture& VirtualTextureSystem::
GetLogicalTextureInfo(unsigned virtual_texture_id) const
{
    return *m_logical_texture_infos.at(virtual_texture_id);
}

std::shared_ptr<VTPhysicalTexture> VirtualTextureSystem::GetSVTPhysicalTexture() const
{
    for (const auto& physical_texture : m_physical_virtual_textures)
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
    for (const auto& physical_texture : m_physical_virtual_textures)
    {
        if (!physical_texture->IsSVT())
        {
            return physical_texture;
        }
    }
    return nullptr;
}

std::pair<unsigned, unsigned> VirtualTextureSystem::GetVTFeedbackTextureSize(const VTLogicalTexture& logical_texture)
{
    return
    {
        static_cast<unsigned>(logical_texture.GetSize()) / VT_FEEDBACK_TEXTURE_SCALE_SIZE,
        static_cast<unsigned>(logical_texture.GetSize()) / VT_FEEDBACK_TEXTURE_SCALE_SIZE,
    };
}

unsigned VirtualTextureSystem::GetAvailableVTIdAndInc()
{
    return m_virtual_texture_id++;
}

VTPageType VirtualTextureSystem::GetPageType(unsigned virtual_texture_id) const
{
    GLTF_CHECK(m_logical_texture_infos.contains(virtual_texture_id));
    return m_logical_texture_infos.at(virtual_texture_id)->IsSVT() ? VTPageType::SVT_PAGE : VTPageType::RVT_PAGE;
}

void VirtualTextureSystem::InitFeedBackPass()
{
}

void VirtualTextureSystem::GatherPageRequest(std::vector<VTPage>& out_pages)
{
    std::set<VTPage> pages;
    for (const auto& feedback_pass  : m_logical_texture_feedback_passes)
    {
        const auto& feedback_data = feedback_pass.second.second->GetFeedbackOutputDataAndReset();
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
            page.logical_tex_id = feedback.data[3];
            page.type = GetPageType(page.logical_tex_id);

            if (!pages.contains(page))
            {
                pages.insert(page);
                out_pages.push_back(page);
            }
        }
    }
}
