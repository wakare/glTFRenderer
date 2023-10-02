#include "glTFGraphicsPassMeshBase.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

glTFGraphicsPassMeshBase::glTFGraphicsPassMeshBase()
    : glTFRenderInterfaceSceneView(MeshBasePass_RootParameter_SceneView_CBV, MeshBasePass_SceneView_CBV_Register)
    , glTFRenderInterfaceSceneMesh(
        MeshBasePass_RootParameter_SceneMesh_CBV,
        MeshBasePass_SceneMesh_CBV_Register)
{
}

bool glTFGraphicsPassMeshBase::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::InitPass(resource_manager))
    RETURN_IF_FALSE(glTFRenderInterfaceSceneView::InitInterface(resource_manager))
    RETURN_IF_FALSE(glTFRenderInterfaceSceneMesh::InitInterface(resource_manager))
    
    return true;
}

bool glTFGraphicsPassMeshBase::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    RHIUtils::Instance().SetPrimitiveTopology( command_list, RHIPrimitiveTopologyType::TRIANGLELIST);

    RETURN_IF_FALSE(glTFRenderInterfaceSceneView::ApplyInterface(resource_manager, GetPipelineType() == PipelineType::Graphics))

    return true;
}

bool glTFGraphicsPassMeshBase::RenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::RenderPass(resource_manager))    

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    // Render meshes
    for (const auto& mesh : m_meshes)
    {
        const glTFUniqueID meshID = mesh.first;
        RETURN_IF_FALSE(BeginDrawMesh(resource_manager, meshID))
        
        // Upload constant buffer
        ConstantBufferSceneMesh temp_mesh_data =
        {
            mesh.second.meshTransformMatrix,
            glm::transpose(glm::inverse(mesh.second.meshTransformMatrix)),
            mesh.second.using_normal_mapping
        };
        
        RETURN_IF_FALSE(glTFRenderInterfaceSceneMesh::UpdateCPUBuffer(&temp_mesh_data, sizeof(temp_mesh_data)))
        
        glTFRenderInterfaceSceneMesh::ApplyInterface(resource_manager, GetPipelineType() == PipelineType::Graphics);

        RHIUtils::Instance().SetVertexBufferView(command_list, *mesh.second.mesh_vertex_buffer_view);
        RHIUtils::Instance().SetIndexBufferView(command_list, *mesh.second.mesh_index_buffer_view);
        
        RHIUtils::Instance().DrawIndexInstanced(command_list,
            mesh.second.mesh_index_count, 1, 0, 0, 0);

        RETURN_IF_FALSE(EndDrawMesh(resource_manager, mesh.first))
    }

    return true;
}

bool glTFGraphicsPassMeshBase::AddOrUpdatePrimitiveToMeshPass(glTFRenderResourceManager& resource_manager, const glTFScenePrimitive& primitive)
{
    const glTFUniqueID mesh_ID = primitive.GetID();
    if (m_meshes.find(mesh_ID) == m_meshes.end())
    {
        // Upload vertex and index data once
        const auto& vertex_buffer = m_meshes[mesh_ID].mesh_vertex_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
        const auto& index_buffer = m_meshes[mesh_ID].mesh_index_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
        const auto& vertex_buffer_view = m_meshes[mesh_ID].mesh_vertex_buffer_view = RHIResourceFactory::CreateRHIResource<IRHIVertexBufferView>();
        const auto& index_buffer_view = m_meshes[mesh_ID].mesh_index_buffer_view = RHIResourceFactory::CreateRHIResource<IRHIIndexBufferView>();
        
        const auto& primitive_vertices = primitive.GetVertexBufferData();
        const auto& primitive_indices = primitive.GetIndexBufferData();

        const RHIBufferDesc vertex_buffer_desc = {L"vertexBufferDefaultBuffer", primitive_vertices.byteSize, 1, 1, RHIBufferType::Default, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
        const RHIBufferDesc index_buffer_desc = {L"indexBufferDefaultBuffer", primitive_indices.byteSize, 1, 1, RHIBufferType::Default, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
    
        RETURN_IF_FALSE(vertex_buffer->InitGPUBuffer(resource_manager.GetDevice(), vertex_buffer_desc ))
        RETURN_IF_FALSE(index_buffer->InitGPUBuffer(resource_manager.GetDevice(), index_buffer_desc ))

        const auto vertex_upload_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
        const auto index_upload_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();

        const RHIBufferDesc vertex_upload_buffer_desc = {L"vertexBufferUploadBuffer", primitive_vertices.byteSize, 1, 1, RHIBufferType::Upload, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
        const RHIBufferDesc index_upload_buffer_desc = {L"indexBufferUploadBuffer", primitive_indices.byteSize, 1, 1, RHIBufferType::Upload, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};

        RETURN_IF_FALSE(vertex_upload_buffer->InitGPUBuffer(resource_manager.GetDevice(), vertex_upload_buffer_desc ))
        RETURN_IF_FALSE(index_upload_buffer->InitGPUBuffer(resource_manager.GetDevice(), index_upload_buffer_desc ))

        auto& command_list = resource_manager.GetCommandListForRecord();
        
        RETURN_IF_FALSE(RHIUtils::Instance().UploadBufferDataToDefaultGPUBuffer(command_list, *vertex_upload_buffer, *vertex_buffer, primitive_vertices.data.get(), primitive_vertices.byteSize))
        RETURN_IF_FALSE(RHIUtils::Instance().UploadBufferDataToDefaultGPUBuffer(command_list, *index_upload_buffer, *index_buffer, primitive_indices.data.get(), primitive_indices.byteSize))
    
        RETURN_IF_FALSE(RHIUtils::Instance().AddBufferBarrierToCommandList(command_list, *vertex_buffer, RHIResourceStateType::COPY_DEST, RHIResourceStateType::VERTEX_AND_CONSTANT_BUFFER))
        RETURN_IF_FALSE(RHIUtils::Instance().AddBufferBarrierToCommandList(command_list, *index_buffer, RHIResourceStateType::COPY_DEST, RHIResourceStateType::INDEX_BUFFER))

        resource_manager.CloseCommandListAndExecute(true);
        
        vertex_buffer_view->InitVertexBufferView(*vertex_buffer, 0, primitive.GetVertexLayout().GetVertexStrideInBytes(), primitive_vertices.byteSize);
        index_buffer_view->InitIndexBufferView(*index_buffer, 0, primitive_indices.elementType == IndexBufferElementType::UNSIGNED_INT ? RHIDataFormat::R32_UINT : RHIDataFormat::R16_UINT, primitive_indices.byteSize);

        m_meshes[mesh_ID].mesh_vertex_count = primitive_vertices.vertex_count;
        m_meshes[mesh_ID].mesh_index_count = primitive_indices.index_count;
        m_meshes[mesh_ID].material_id = primitive.HasMaterial() ? primitive.GetMaterial().GetID() : glTFUniqueIDInvalid;
        m_meshes[mesh_ID].using_normal_mapping = primitive.HasNormalMapping();
    }

    // Only update when transform has changed
    m_meshes[mesh_ID].meshTransformMatrix = primitive.GetTransformMatrix();
    
    return true; 
}

bool glTFGraphicsPassMeshBase::RemovePrimitiveFromMeshPass(glTFUniqueID mesh_id_to_remove)
{
    if (auto it = (m_meshes.find(mesh_id_to_remove)) != m_meshes.end())
    {
        LOG_FORMAT("[DEBUG] Remove mesh id %d", mesh_id_to_remove)
        m_meshes.erase(it);
    }
    else
    {
        LOG_FORMAT("[WARN] Can not find meshID in Pass")
        assert(false);
    }

    return true;
}

bool glTFGraphicsPassMeshBase::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{   
    
    RETURN_IF_FALSE(glTFRenderInterfaceSceneView::ApplyRootSignature(*m_root_signature))
    RETURN_IF_FALSE(glTFRenderInterfaceSceneMesh::ApplyRootSignature(*m_root_signature))
    
    return true;
}

bool glTFGraphicsPassMeshBase::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::SetupPipelineStateObject(resource_manager))

    auto& shader_macros = GetGraphicsPipelineStateObject().GetShaderMacros();
    glTFRenderInterfaceSceneView::UpdateShaderCompileDefine(shader_macros);
    glTFRenderInterfaceSceneMesh::UpdateShaderCompileDefine(shader_macros);
    
    return true;
}

bool glTFGraphicsPassMeshBase::BeginDrawMesh(glTFRenderResourceManager& resourceManager, glTFUniqueID meshID)
{
    return true;   
}

bool glTFGraphicsPassMeshBase::EndDrawMesh(glTFRenderResourceManager& resourceManager, glTFUniqueID meshID)
{
    return true;
}

std::vector<RHIPipelineInputLayout> glTFGraphicsPassMeshBase::GetVertexInputLayout()
{
    return m_vertex_input_layouts;
}

bool glTFGraphicsPassMeshBase::TryProcessSceneObject(glTFRenderResourceManager& resourceManager, const glTFSceneObjectBase& object)
{
    const glTFScenePrimitive* primitive = dynamic_cast<const glTFScenePrimitive*>(&object);
    
    if (!primitive || !primitive->IsVisible())
    {
        return false;
    }

    return AddOrUpdatePrimitiveToMeshPass(resourceManager, *primitive) &&
        primitive->HasMaterial() && ProcessMaterial(resourceManager, primitive->GetMaterial());
}

bool glTFGraphicsPassMeshBase::ResolveVertexInputLayout(const VertexLayoutDeclaration& source_vertex_layout)
{
	m_vertex_input_layouts.clear();
    
    unsigned vertex_layout_offset = 0;
    for (const auto& vertex_layout : source_vertex_layout.elements)
    {
        switch (vertex_layout.type)
        {
        case VertexLayoutType::POSITION:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetRHIDataFormatBitsPerPixel(RHIDataFormat::R32G32B32_FLOAT) / 8));
                m_vertex_input_layouts.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(POSITION), 0, RHIDataFormat::R32G32B32_FLOAT, vertex_layout_offset});
            }
            break;
        case VertexLayoutType::NORMAL:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetRHIDataFormatBitsPerPixel(RHIDataFormat::R32G32B32_FLOAT) / 8));
                m_vertex_input_layouts.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(NORMAL), 0, RHIDataFormat::R32G32B32_FLOAT, vertex_layout_offset});
            }
            break;
        case VertexLayoutType::TANGENT:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetRHIDataFormatBitsPerPixel(RHIDataFormat::R32G32B32A32_FLOAT) / 8));
                m_vertex_input_layouts.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(TANGENT), 0, RHIDataFormat::R32G32B32A32_FLOAT, vertex_layout_offset});
            }
            break;
        case VertexLayoutType::TEXCOORD_0:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetRHIDataFormatBitsPerPixel(RHIDataFormat::R32G32_FLOAT) / 8));
                m_vertex_input_layouts.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(TEXCOORD), 0, RHIDataFormat::R32G32_FLOAT, vertex_layout_offset});
            }
            break;
            // TODO: Handle TEXCOORD_1?
        }

        vertex_layout_offset += vertex_layout.byte_size;   
    }

    return true;
}

size_t glTFGraphicsPassMeshBase::GetRootSignatureParameterCount()
{
    return MeshBasePass_RootParameter_LastIndex;
}

size_t glTFGraphicsPassMeshBase::GetRootSignatureSamplerCount()
{
    return 1;
}
