#include "VirtualTextureSystem.h"

#include "glTFRenderPass/glTFRenderPassManager.h"
#include "glTFRenderPass/glTFComputePass/glTFComputePassVTFetchCS.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassMeshVTFeedback.h"

bool VirtualTextureSystem::InitRenderSystem(glTFRenderResourceManager& resource_manager)
{
    m_page_streamer = std::make_shared<VTPageStreamer>();
    m_physical_svt_texture = std::make_shared<VTPhysicalTexture>(VT_PHYSICAL_TEXTURE_SIZE, VT_PAGE_SIZE, VT_PHYSICAL_TEXTURE_BORDER, true);
    m_physical_rvt_texture = std::make_shared<VTPhysicalTexture>(VT_PHYSICAL_TEXTURE_SIZE, VT_PAGE_SIZE, VT_PHYSICAL_TEXTURE_BORDER, false);

    m_physical_svt_texture->UpdateRenderResource(resource_manager);
    m_physical_rvt_texture->UpdateRenderResource(resource_manager);

    return true;
}

void VirtualTextureSystem::SetupPass(glTFRenderResourceManager& resource_manager, glTFRenderPassManager& pass_manager, const glTFSceneGraph& scene_graph)
{
    for (const auto& logic_texture : m_logical_texture_infos)
    {
        auto feedback_pass = std::make_shared<glTFGraphicsPassMeshVTFeedback>(logic_texture.first, !!dynamic_cast<VTShadowmapLogicalTexture*>(logic_texture.second.first.get()));
        pass_manager.AddRenderPass(POST_SCENE, feedback_pass);
        
        auto fetch_feedback_pass = std::make_shared<glTFComputePassVTFetchCS>(logic_texture.first);
        pass_manager.AddRenderPass(POST_SCENE, fetch_feedback_pass);

        m_logical_texture_feedback_passes[logic_texture.first] = {feedback_pass, fetch_feedback_pass};

        logic_texture.second.second->UpdateRenderResource(resource_manager);
    }
    
    InitFeedBackPass();
}

void VirtualTextureSystem::ShutdownRenderSystem()
{
    
}

void VirtualTextureSystem::TickRenderSystem(glTFRenderResourceManager& resource_manager)
{
    UpdateRenderResource(resource_manager);
    
    std::vector<VTPage> pages;
    GatherPageRequest(pages);

    for (const auto& page : pages)
    {
        m_page_streamer->AddPageRequest(page);    
    }
    
    m_page_streamer->Tick();
    
    std::vector<VTPageData> svt_page_data;
    std::vector<VTPageData> rvt_page_data;
    
    {
        auto page_data = m_page_streamer->GetRequestResultsAndClean();
        for (const auto& page : page_data)
        {
            if (m_logical_texture_infos.at(page.page.tex).first->IsSVT() )
            {
                svt_page_data.push_back(page);
            }
            else
            {
                rvt_page_data.push_back(page);
            }
        }    
    }
    
    if (!svt_page_data.empty())
    {
        m_physical_svt_texture->ProcessRequestResult(svt_page_data);

        // Update all page table based physical texture allocation info
        const auto& page_allocations = m_physical_svt_texture->GetPageAllocationInfos();

        for (auto& logical_texture_info : m_logical_texture_infos)
        {
            auto& logical_texture = logical_texture_info.second.first;
            if (!logical_texture->IsSVT())
            {
                continue;
            }
        
            auto& page_table  = logical_texture_info.second.second;
            page_table->Invalidate();
        
            for (const auto& page_allocation : page_allocations)
            {
                if (page_allocation.second.page.tex == page_table->GetTextureId())
                {
                    page_table->TouchPageAllocation(page_allocation.second);
                }
            }
            page_table->UpdateTextureData();
            page_table->UpdateRenderResource(resource_manager);
            logical_texture->UpdateRenderResource(resource_manager);
        }

        m_physical_svt_texture->UpdateTextureData();
        m_physical_svt_texture->UpdateRenderResource(resource_manager);
    }

    if (!rvt_page_data.empty())
    {
        m_physical_rvt_texture->ProcessRequestResult(rvt_page_data);
        // Update all page table based physical texture allocation info
        const auto& page_allocations = m_physical_rvt_texture->GetPageAllocationInfos();
        for (auto& logical_texture_info : m_logical_texture_infos)
        {
            auto& logical_texture = logical_texture_info.second.first;
            if (logical_texture->IsSVT())
            {
                continue;
            }

            auto& page_table  = logical_texture_info.second.second;
            page_table->Invalidate();
        
            for (const auto& page_allocation : page_allocations)
            {
                if (page_allocation.second.page.tex == page_table->GetTextureId())
                {
                    page_table->TouchPageAllocation(page_allocation.second);
                }
            }
            page_table->UpdateTextureData();
            page_table->UpdateRenderResource(resource_manager);
            logical_texture->UpdateRenderResource(resource_manager);
        }
        //m_physical_rvt_texture->UpdateTextureData();
        m_physical_rvt_texture->UpdateRenderResource(resource_manager);
    }
}

bool VirtualTextureSystem::HasTexture(std::shared_ptr<VTLogicalTexture> texture) const
{
    return m_logical_texture_infos.contains(texture->GetTextureId());
}

bool VirtualTextureSystem::RegisterTexture(std::shared_ptr<VTLogicalTexture> texture)
{
    std::shared_ptr<VTPageTable> page_table = std::make_shared<VTPageTable>();
    const int page_table_size = texture->GetSize() / VT_PAGE_SIZE;
    page_table->InitVTPageTable(texture->GetTextureId(), page_table_size, VT_PAGE_SIZE);
    m_logical_texture_infos[texture->GetTextureId()] = std::make_pair(texture, page_table);
    m_page_streamer->AddLogicalTexture(texture);
    
    return true;
}

bool VirtualTextureSystem::UpdateRenderResource(glTFRenderResourceManager& resource_manager)
{
    return true;
}

const std::map<int, std::pair<std::shared_ptr<VTLogicalTexture>, std::shared_ptr<VTPageTable>>>& VirtualTextureSystem::
GetLogicalTextureInfos() const
{
    return m_logical_texture_infos;
}

const std::pair<std::shared_ptr<VTLogicalTexture>, std::shared_ptr<VTPageTable>>& VirtualTextureSystem::
GetLogicalTextureInfo(unsigned virtual_texture_id) const
{
    return m_logical_texture_infos.at(virtual_texture_id);
}

std::shared_ptr<VTPhysicalTexture> VirtualTextureSystem::GetSVTPhysicalTexture() const
{
    return m_physical_svt_texture;
}

std::shared_ptr<VTPhysicalTexture> VirtualTextureSystem::GetRVTPhysicalTexture() const
{
    return m_physical_rvt_texture;
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
            page.tex = feedback.data[3];

            if (!pages.contains(page))
            {
                pages.insert(page);
                out_pages.push_back(page);
            }
        }
    }
}
