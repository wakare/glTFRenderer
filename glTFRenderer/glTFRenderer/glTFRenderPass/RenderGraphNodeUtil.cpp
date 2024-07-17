#include "RenderGraphNodeUtil.h"

#include "glTFRenderResourceManager.h"

std::shared_ptr<IRHITexture> RenderGraphNodeUtil::RenderGraphNode::GetResourceTexture(RenderPassResourceTableId id) const
{
    auto find_texture_allocation = m_resource_location.m_texture_allocations.find(id);
    GLTF_CHECK(find_texture_allocation != m_resource_location.m_texture_allocations.end() &&
        find_texture_allocation->second);

    return find_texture_allocation->second;
}

void RenderGraphNodeUtil::RenderGraphNode::AddImportTextureResource(const RHITextureDesc& desc,
    RenderPassResourceTableId id)
{
    m_resource_table.m_import_texture_table_entry.emplace_back(desc, id);
}

void RenderGraphNodeUtil::RenderGraphNode::AddExportTextureResource(const RHITextureDesc& desc,
    RenderPassResourceTableId id)
{
    m_resource_table.m_export_texture_table_entry.emplace_back(desc, id);
}

bool RenderGraphNodeUtil::RenderGraphNode::InitResourceTable(glTFRenderResourceManager& resource_manager)
{
    return true;
}

bool RenderGraphNodeUtil::RenderGraphNode::ExportResourceLocation(glTFRenderResourceManager& resource_manager)
{
    const auto& resource_table = GetResourceTable();
    for (const auto& export_texture_info : resource_table.m_export_texture_table_entry)
    {
        std::shared_ptr<IRHITexture> out_texture_allocation;
        const bool exported = resource_manager.ExportResourceTexture(export_texture_info.first, export_texture_info.second, out_texture_allocation);
        GLTF_CHECK(exported);

        m_resource_location.m_texture_allocations[export_texture_info.second] = out_texture_allocation;
    }
    
    return true;
}

bool RenderGraphNodeUtil::RenderGraphNode::ImportResourceLocation(glTFRenderResourceManager& resource_manager)
{
    const auto& resource_table = GetResourceTable();
    for (const auto& import_texture_info : resource_table.m_import_texture_table_entry)
    {
        std::shared_ptr<IRHITexture> out_texture_allocation;
        const bool imported = resource_manager.ImportResourceTexture(import_texture_info.first, import_texture_info.second, out_texture_allocation);
        GLTF_CHECK(imported);

        m_resource_location.m_texture_allocations[import_texture_info.second] = out_texture_allocation;
    }
    
    return true;
}

const RenderPassResourceTable& RenderGraphNodeUtil::RenderGraphNode::GetResourceTable() const
{
    return m_resource_table;
}
