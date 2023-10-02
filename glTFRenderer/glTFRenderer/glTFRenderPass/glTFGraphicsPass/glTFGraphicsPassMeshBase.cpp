#include "glTFGraphicsPassMeshBase.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "glTFRHI/RHIInterface/IRHIVertexBuffer.h"

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
    auto& device = resource_manager.GetDevice();
    auto& command_list = resource_manager.GetCommandListForRecord();
    
    const glTFUniqueID mesh_ID = primitive.GetID();
    if (m_meshes.find(mesh_ID) == m_meshes.end())
    {
        m_meshes[mesh_ID].mesh_vertex_buffer = RHIResourceFactory::CreateRHIResource<IRHIVertexBuffer>();
        const RHIBufferDesc vertex_buffer_desc = {L"vertexBufferDefaultBuffer", primitive.GetVertexBufferData().byteSize, 1, 1, RHIBufferType::Default, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
        m_meshes[mesh_ID].mesh_vertex_buffer_view = m_meshes[mesh_ID].mesh_vertex_buffer->CreateVertexBufferView(device, command_list, vertex_buffer_desc, primitive.GetVertexBufferData());

        m_meshes[mesh_ID].mesh_position_only_buffer = RHIResourceFactory::CreateRHIResource<IRHIVertexBuffer>();
        const RHIBufferDesc position_only_buffer_desc = {L"positionOnlyBufferDefaultBuffer", primitive.GetPositionOnlyBufferData().byteSize, 1, 1, RHIBufferType::Default, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
        m_meshes[mesh_ID].mesh_position_only_buffer_view = m_meshes[mesh_ID].mesh_position_only_buffer->CreateVertexBufferView(device, command_list, position_only_buffer_desc, primitive.GetPositionOnlyBufferData());

        m_meshes[mesh_ID].mesh_index_buffer = RHIResourceFactory::CreateRHIResource<IRHIIndexBuffer>();
        const RHIBufferDesc index_buffer_desc = {L"indexBufferDefaultBuffer", primitive.GetIndexBufferData().byteSize, 1, 1, RHIBufferType::Default, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
        m_meshes[mesh_ID].mesh_index_buffer_view = m_meshes[mesh_ID].mesh_index_buffer->CreateIndexBufferView(device, command_list, index_buffer_desc, primitive.GetIndexBufferData());
       
        m_meshes[mesh_ID].mesh_vertex_count = primitive.GetVertexBufferData().vertex_count;
        m_meshes[mesh_ID].mesh_index_count = primitive.GetIndexBufferData().index_count;
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
