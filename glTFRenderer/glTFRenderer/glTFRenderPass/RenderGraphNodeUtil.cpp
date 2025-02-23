#include "RenderGraphNodeUtil.h"
#include "glTFRenderResourceManager.h"

std::shared_ptr<IRHITexture> RenderGraphNodeUtil::RenderGraphNode::GetResourceTexture(RenderPassResourceTableId id) const
{
    auto find_texture_allocation = m_resource_location.m_texture_allocations.find(id);
    GLTF_CHECK(find_texture_allocation != m_resource_location.m_texture_allocations.end() &&
        find_texture_allocation->second[m_current_frame_index]);

    return find_texture_allocation->second[m_current_frame_index];
}

std::shared_ptr<IRHITextureDescriptorAllocation> RenderGraphNodeUtil::RenderGraphNode::GetResourceDescriptor(
    RenderPassResourceTableId id) const
{
    auto find_texture_allocation = m_resource_location.m_texture_descriptor_allocations.find(id);
    GLTF_CHECK(find_texture_allocation != m_resource_location.m_texture_descriptor_allocations.end() &&
        find_texture_allocation->second[m_current_frame_index]);

    return find_texture_allocation->second[m_current_frame_index];
}

void RenderGraphNodeUtil::RenderGraphNode::AddImportTextureResource(RenderPassResourceTableId id,
                                                                    const RHITextureDesc& desc, const RHITextureDescriptorDesc& descriptor_desc)
{
    m_resource_table.m_import_texture_table_entry.emplace_back(desc, id);

    GLTF_CHECK(!m_resource_table.m_import_descriptor_table_entry.contains(id));
    m_resource_table.m_import_descriptor_table_entry.emplace(id, descriptor_desc);
}

void RenderGraphNodeUtil::RenderGraphNode::AddExportTextureResource(RenderPassResourceTableId id,
                                                                    const RHITextureDesc& texture_desc, const RHITextureDescriptorDesc& descriptor_desc)
{
    m_resource_table.m_export_texture_table_entry.emplace_back(texture_desc, id);
    
    GLTF_CHECK(!m_resource_table.m_export_descriptor_table_entry.contains(id));
    m_resource_table.m_export_descriptor_table_entry.emplace(id, descriptor_desc);
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
        std::vector<std::shared_ptr<IRHITexture>> out_texture_allocation;
        const bool exported = resource_manager.ExportResourceTexture(export_texture_info.first, export_texture_info.second, out_texture_allocation);
        GLTF_CHECK(exported);

        m_resource_location.m_texture_allocations[export_texture_info.second] = out_texture_allocation;    
    }

    for (const auto& export_descriptor : resource_table.m_export_descriptor_table_entry)
    {
        const auto& textures = m_resource_location.m_texture_allocations[export_descriptor.first];
        for (const auto& texture : textures)
        {
            std::shared_ptr<IRHITextureDescriptorAllocation> texture_descriptor_allocation = nullptr;
            resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), texture,
                            export_descriptor.second, texture_descriptor_allocation);
            m_resource_location.m_texture_descriptor_allocations[export_descriptor.first].push_back(texture_descriptor_allocation);   
        }
    }
    
    return true;
}

bool RenderGraphNodeUtil::RenderGraphNode::ImportResourceLocation(glTFRenderResourceManager& resource_manager)
{
    const auto& resource_table = GetResourceTable();
    for (const auto& import_texture_info : resource_table.m_import_texture_table_entry)
    {
        std::vector<std::shared_ptr<IRHITexture>> out_texture_allocations;
        const bool imported = resource_manager.ImportResourceTexture(import_texture_info.first, import_texture_info.second, out_texture_allocations);
        GLTF_CHECK(imported);

        m_resource_location.m_texture_allocations[import_texture_info.second] = out_texture_allocations;
    }

    for (const auto& import_descriptor : resource_table.m_import_descriptor_table_entry)
    {
        const auto& textures = m_resource_location.m_texture_allocations[import_descriptor.first];
        for (const auto& texture : textures)
        {
            std::shared_ptr<IRHITextureDescriptorAllocation> texture_descriptor_allocation = nullptr;
            resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), texture,
                            import_descriptor.second, texture_descriptor_allocation);
            m_resource_location.m_texture_descriptor_allocations[import_descriptor.first].push_back(texture_descriptor_allocation);   
        }
    }
    
    return true;
}

const RenderPassResourceTable& RenderGraphNodeUtil::RenderGraphNode::GetResourceTable() const
{
    return m_resource_table;
}

void RenderGraphNodeUtil::RenderGraphNode::UpdateFrameIndex(const glTFRenderResourceManager& resource_manager)
{
    m_current_frame_index = resource_manager.GetCurrentBackBufferIndex();
}

void RenderGraphNodeUtil::RenderGraphNode::AddFinalOutputCandidate(RenderPassResourceTableId id)
{
    m_final_output_candidates.push_back(id);
}
