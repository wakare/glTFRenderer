#include "VirtualTextureSystem.h"

#include "glTFRenderPass/glTFRenderPassManager.h"
#include "glTFRenderPass/glTFComputePass/glTFComputePassVTFetchCS.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassMeshVTFeedback.h"
#include "glTFRHI/RHIInterface/IRHISwapChain.h"

bool VirtualTextureSystem::InitRenderSystem(glTFRenderResourceManager& resource_manager)
{
    m_page_streamer = std::make_shared<VTPageStreamer>();
    m_physical_texture = std::make_shared<VTPhysicalTexture>(VT_PHYSICAL_TEXTURE_SIZE, VT_PAGE_SIZE, VT_PHYSICAL_TEXTURE_BORDER);

    return true;
}

void VirtualTextureSystem::SetupPass(glTFRenderPassManager& pass_manager, const glTFSceneGraph& scene_graph)
{
    for (const auto& logic_texture : m_logical_texture_infos)
    {
        auto feedback_pass = std::make_shared<glTFGraphicsPassMeshVTFeedback>(logic_texture.first);
        auto fetch_feedback_pass = std::make_shared<glTFComputePassVTFetchCS>(logic_texture.first);
    
        pass_manager.AddRenderPass(POST_SCENE, feedback_pass);
        pass_manager.AddRenderPass(POST_SCENE, fetch_feedback_pass);

        m_logical_texture_feedback_passes[logic_texture.first] = {feedback_pass, fetch_feedback_pass};
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
    auto page_data = m_page_streamer->GetRequestResultsAndClean();

    m_physical_texture->ProcessRequestResult(page_data);

    // Update all page table based physical texture allocation info
    const auto& page_allocations = m_physical_texture->GetPageAllocationInfos();

    for (auto& logical_texture_info : m_logical_texture_infos)
    {
        auto& page_table  = logical_texture_info.second.second;
        auto& logical_texture = logical_texture_info.second.first;
        page_table->Invalidate();
        
        for (const auto& page_allocation : page_allocations)
        {
            if (page_allocation.second.page.tex == page_table->GetTextureId())
            {
                page_table->TouchPageAllocation(page_allocation.second);
            }
        }
        page_table->UpdateTextureData();
        logical_texture->UpdateRenderResource(resource_manager);
    }

    // Upload page table to texture resource
    for (auto& logical_texture_info : m_logical_texture_infos)
    {
        auto& page_table  = logical_texture_info.second.second;
        page_table->UpdateRenderResource(resource_manager);
    }
    
    m_physical_texture->UpdateTextureData();
    m_physical_texture->UpdateRenderResource(resource_manager);
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

std::shared_ptr<VTPhysicalTexture> VirtualTextureSystem::GetPhysicalTexture() const
{
    return m_physical_texture;
}

std::pair<unsigned, unsigned> VirtualTextureSystem::GetVTFeedbackTextureSize(glTFRenderResourceManager& resource_manager)
{
    return
    {
        resource_manager.GetSwapChain().GetWidth() / VT_FEEDBACK_TEXTURE_SCALE_SIZE,
        resource_manager.GetSwapChain().GetHeight() / VT_FEEDBACK_TEXTURE_SCALE_SIZE,
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
