#include "glTFGraphicsPassMeshBase.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

glTFGraphicsPassMeshBase::glTFGraphicsPassMeshBase()
{
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneView>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneMesh>());
}

bool glTFGraphicsPassMeshBase::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::InitPass(resource_manager))
    
    return true;
}

bool glTFGraphicsPassMeshBase::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    RHIUtils::Instance().SetPrimitiveTopology( command_list, RHIPrimitiveTopologyType::TRIANGLELIST);

    return true;
}

bool glTFGraphicsPassMeshBase::RenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::RenderPass(resource_manager))    

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    // Render meshes
    const auto& mesh_render_resources = resource_manager.GetMeshManager().GetMeshRenderResources();
    for (const auto& mesh_instance : resource_manager.GetMeshManager().GetMeshInstanceRenderResource())
    {
        const auto& instance_data = mesh_render_resources.find(mesh_instance.second.m_mesh_render_resource);
        if (instance_data == mesh_render_resources.end())
        {
            // No valid instance data exists..
            continue;
        }
        
        if (!instance_data->second.mesh->IsVisible())
        {
            continue;
        }
        
        const glTFUniqueID mesh_id = mesh_instance.first;
        RETURN_IF_FALSE(BeginDrawMesh(resource_manager, instance_data->second, mesh_instance.second))
        
        RHIUtils::Instance().SetVertexBufferView(command_list, *instance_data->second.mesh_vertex_buffer_view);
        RHIUtils::Instance().SetIndexBufferView(command_list, *instance_data->second.mesh_index_buffer_view);
        
        RHIUtils::Instance().DrawIndexInstanced(command_list,
            instance_data->second.mesh_index_count, 1, 0, 0, 0);

        RETURN_IF_FALSE(EndDrawMesh(resource_manager, instance_data->second, mesh_instance.second))
    }

    return true;
}

bool glTFGraphicsPassMeshBase::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::SetupRootSignature(resource_manager))
    
    return true;
}

bool glTFGraphicsPassMeshBase::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::SetupPipelineStateObject(resource_manager))

    return true;
}

bool glTFGraphicsPassMeshBase::BeginDrawMesh(glTFRenderResourceManager& resource_manager, const glTFMeshRenderResource& mesh_data, const glTFMeshInstanceRenderResource& instance_data)
{
    // Upload constant buffer
    ConstantBufferSceneMesh temp_mesh_data =
    {
        instance_data.m_instance_transform,
        glm::transpose(glm::inverse(instance_data.m_instance_transform)),
        mesh_data.material_id, mesh_data.using_normal_mapping
    };
        
    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceSceneMesh>()->UploadAndApplyDataWithIndex(resource_manager, instance_data.m_mesh_render_resource, temp_mesh_data, true))
    
    return true;   
}

bool glTFGraphicsPassMeshBase::EndDrawMesh(glTFRenderResourceManager& resource_manager, const glTFMeshRenderResource& mesh_data, const glTFMeshInstanceRenderResource& instance_data)
{
    return true;
}

std::vector<RHIPipelineInputLayout> glTFGraphicsPassMeshBase::GetVertexInputLayout()
{
    return m_vertex_input_layouts;
}

bool glTFGraphicsPassMeshBase::TryProcessSceneObject(glTFRenderResourceManager& resourceManager, const glTFSceneObjectBase& object)
{
    return true;
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