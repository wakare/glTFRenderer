#include "VirtualTextureSystem.h"

#include "glTFRenderPass/glTFRenderPassManager.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassMeshVT.h"

bool VirtualTextureSystem::InitRenderSystem(glTFRenderResourceManager& resource_manager)
{
    m_page_streamer = std::make_shared<VTPageStreamer>();
    m_physical_texture = std::make_shared<VTPhysicalTexture>(2048, 64, 1);
    m_feedback_pass = std::make_shared<glTFGraphicsPassMeshVT>();
    
    return true;
}

void VirtualTextureSystem::SetupPass(glTFRenderPassManager& pass_manager)
{
    pass_manager.AddRenderPass(m_feedback_pass);
}

void VirtualTextureSystem::ShutdownRenderSystem()
{
    
}

void VirtualTextureSystem::TickRenderSystem(glTFRenderResourceManager& resource_manager)
{
    DrawFeedBackPass();

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
        page_table->Invalidate();
        for (const auto& page_allocation : page_allocations)
        {
            if (page_allocation.second.page.tex == page_table->GetTextureId())
            {
                page_table->TouchPageAllocation(page_allocation.second);
            }
        }
        page_table->UpdateTextureData();
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

bool VirtualTextureSystem::RegisterTexture(std::shared_ptr<VTLogicalTexture> texture)
{
    std::shared_ptr<VTPageTable> page_table = std::make_shared<VTPageTable>();
    const int page_table_size = texture->GetSize() / VT_PAGE_SIZE;
    page_table->InitVTPageTable(texture->GetTextureId(), page_table_size, VT_PAGE_SIZE);
    m_logical_texture_infos[texture->GetTextureId()] = std::make_pair(texture, page_table);
    
    return true;
}

bool VirtualTextureSystem::InitRenderResource(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(m_physical_texture->InitRenderResource(resource_manager));
    
    for (const auto& logical_texture_info : m_logical_texture_infos)
    {
        RETURN_IF_FALSE(logical_texture_info.second.second->InitRenderResource(resource_manager));
    }

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

void VirtualTextureSystem::DrawFeedBackPass()
{
}

void VirtualTextureSystem::GatherPageRequest(std::vector<VTPage>& out_pages)
{
}
