#include "glTFRenderMeshManager.h"

#include "glTFRenderResourceManager.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

bool glTFRenderMeshManager::AddOrUpdatePrimitive(glTFRenderResourceManager& resource_manager,
                                                 const glTFScenePrimitive* primitive)
{
    auto& device = resource_manager.GetDevice();
    auto& command_list = resource_manager.GetCommandListForRecord();
    
    const glTFUniqueID mesh_ID = primitive->GetMeshRawDataID();
    const glTFUniqueID instance_ID = primitive->GetID();
    
    if (m_mesh_render_resources.find(mesh_ID) == m_mesh_render_resources.end())
    {
        m_mesh_render_resources[mesh_ID].mesh = primitive;
        m_mesh_render_resources[mesh_ID].mesh_vertex_buffer = RHIResourceFactory::CreateRHIResource<IRHIVertexBuffer>();
        const RHIBufferDesc vertex_buffer_desc = {L"vertexBufferDefaultBuffer", primitive->GetVertexBufferData().byteSize, 1, 1, RHIBufferType::Default, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
        m_mesh_render_resources[mesh_ID].mesh_vertex_buffer_view = m_mesh_render_resources[mesh_ID].mesh_vertex_buffer->CreateVertexBufferView(device, command_list, vertex_buffer_desc, primitive->GetVertexBufferData());

        m_mesh_render_resources[mesh_ID].mesh_position_only_buffer = RHIResourceFactory::CreateRHIResource<IRHIVertexBuffer>();
        const RHIBufferDesc position_only_buffer_desc = {L"positionOnlyBufferDefaultBuffer", primitive->GetPositionOnlyBufferData().byteSize, 1, 1, RHIBufferType::Default, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
        m_mesh_render_resources[mesh_ID].mesh_position_only_buffer_view = m_mesh_render_resources[mesh_ID].mesh_position_only_buffer->CreateVertexBufferView(device, command_list, position_only_buffer_desc, primitive->GetPositionOnlyBufferData());

        m_mesh_render_resources[mesh_ID].mesh_index_buffer = RHIResourceFactory::CreateRHIResource<IRHIIndexBuffer>();
        const RHIBufferDesc index_buffer_desc = {L"indexBufferDefaultBuffer", primitive->GetIndexBufferData().byteSize, 1, 1, RHIBufferType::Default, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
        m_mesh_render_resources[mesh_ID].mesh_index_buffer_view = m_mesh_render_resources[mesh_ID].mesh_index_buffer->CreateIndexBufferView(device, command_list, index_buffer_desc, primitive->GetIndexBufferData());
       
        m_mesh_render_resources[mesh_ID].mesh_vertex_count = primitive->GetVertexBufferData().vertex_count;
        m_mesh_render_resources[mesh_ID].mesh_index_count = primitive->GetIndexBufferData().index_count;
        m_mesh_render_resources[mesh_ID].material_id = primitive->HasMaterial() ? primitive->GetMaterial().GetID() : glTFUniqueIDInvalid;
        m_mesh_render_resources[mesh_ID].using_normal_mapping = primitive->HasNormalMapping();
    }

    // Only update when transform has changed
    m_mesh_instances[instance_ID].m_instance_transform = transpose(primitive->GetTransformMatrix());
    m_mesh_instances[instance_ID].m_instance_material_id = primitive->HasMaterial() ? primitive->GetMaterial().GetID() : glTFUniqueIDInvalid;
    m_mesh_instances[instance_ID].m_normal_mapping = primitive->HasNormalMapping();
    m_mesh_instances[instance_ID].m_mesh_render_resource = mesh_ID;
    
    return true; 
}
