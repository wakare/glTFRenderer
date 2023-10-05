#include "glTFRenderMeshManager.h"

#include "glTFRenderResourceManager.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

bool glTFRenderMeshManager::AddOrUpdatePrimitive(glTFRenderResourceManager& resource_manager,
                                                 const glTFScenePrimitive* primitive)
{
    auto& device = resource_manager.GetDevice();
    auto& command_list = resource_manager.GetCommandListForRecord();
    
    const glTFUniqueID mesh_ID = primitive->GetID();
    if (m_meshes.find(mesh_ID) == m_meshes.end())
    {
        m_meshes[mesh_ID].mesh = primitive;
        m_meshes[mesh_ID].mesh_vertex_buffer = RHIResourceFactory::CreateRHIResource<IRHIVertexBuffer>();
        const RHIBufferDesc vertex_buffer_desc = {L"vertexBufferDefaultBuffer", primitive->GetVertexBufferData().byteSize, 1, 1, RHIBufferType::Default, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
        m_meshes[mesh_ID].mesh_vertex_buffer_view = m_meshes[mesh_ID].mesh_vertex_buffer->CreateVertexBufferView(device, command_list, vertex_buffer_desc, primitive->GetVertexBufferData());

        m_meshes[mesh_ID].mesh_position_only_buffer = RHIResourceFactory::CreateRHIResource<IRHIVertexBuffer>();
        const RHIBufferDesc position_only_buffer_desc = {L"positionOnlyBufferDefaultBuffer", primitive->GetPositionOnlyBufferData().byteSize, 1, 1, RHIBufferType::Default, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
        m_meshes[mesh_ID].mesh_position_only_buffer_view = m_meshes[mesh_ID].mesh_position_only_buffer->CreateVertexBufferView(device, command_list, position_only_buffer_desc, primitive->GetPositionOnlyBufferData());

        m_meshes[mesh_ID].mesh_index_buffer = RHIResourceFactory::CreateRHIResource<IRHIIndexBuffer>();
        const RHIBufferDesc index_buffer_desc = {L"indexBufferDefaultBuffer", primitive->GetIndexBufferData().byteSize, 1, 1, RHIBufferType::Default, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
        m_meshes[mesh_ID].mesh_index_buffer_view = m_meshes[mesh_ID].mesh_index_buffer->CreateIndexBufferView(device, command_list, index_buffer_desc, primitive->GetIndexBufferData());
       
        m_meshes[mesh_ID].mesh_vertex_count = primitive->GetVertexBufferData().vertex_count;
        m_meshes[mesh_ID].mesh_index_count = primitive->GetIndexBufferData().index_count;
        m_meshes[mesh_ID].material_id = primitive->HasMaterial() ? primitive->GetMaterial().GetID() : glTFUniqueIDInvalid;
        m_meshes[mesh_ID].using_normal_mapping = primitive->HasNormalMapping();
    }

    // Only update when transform has changed
    m_meshes[mesh_ID].mesh_transform_matrix = primitive->GetTransformMatrix();
    
    return true; 
}
