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

    if (!m_instance_buffer)
    {
        // Construct instance buffer (64MB means contains 1M instance transform)
        static unsigned max_instance_buffer_size = 1024 * 1024 * 64;
        
        m_instance_buffer = RHIResourceFactory::CreateRHIResource<IRHIVertexBuffer>();    
    }
    
    return true;
}

bool glTFGraphicsPassMeshBase::RenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::RenderPass(resource_manager))    

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    // Render meshes
    const auto& mesh_render_resources = resource_manager.GetMeshManager().GetMeshRenderResources();
    const auto& mesh_instance_render_resource = resource_manager.GetMeshManager().GetMeshInstanceRenderResource();
    // mesh id -- <instance count, instance start offset>
    std::map<glTFUniqueID, std::pair<unsigned, unsigned>> mesh_instance_infos;
    
    VertexBufferData instance_buffer;
    instance_buffer.layout.elements.push_back({VertexLayoutType::INSTANCE_MAT_0, sizeof(float) * 4});
    instance_buffer.layout.elements.push_back({VertexLayoutType::INSTANCE_MAT_1, sizeof(float) * 4});
    instance_buffer.layout.elements.push_back({VertexLayoutType::INSTANCE_MAT_2, sizeof(float) * 4});
    instance_buffer.layout.elements.push_back({VertexLayoutType::INSTANCE_MAT_3, sizeof(float) * 4});

    instance_buffer.byteSize = mesh_instance_render_resource.size() * sizeof(glm::mat4);
    instance_buffer.vertex_count = mesh_instance_render_resource.size();
    
    instance_buffer.data = std::make_unique<char[]>(instance_buffer.byteSize);
    size_t instance_index = 0;
    
    for (const auto& mesh : mesh_render_resources)
    {
        auto mesh_ID = mesh.first;
        
        std::vector<glTFUniqueID> instances;
        // Find all instances with current mesh id
        for (const auto& instance : mesh_instance_render_resource)
        {
            if (instance.second.m_mesh_render_resource == mesh_ID)
            {
                instances.push_back(instance.first);
            }
        }

        mesh_instance_infos[mesh_ID] = {instances.size(), instance_index/* * sizeof(glm::mat4)*/};
        
        for (const auto& instance : instances)
        {
            auto instance_data = mesh_instance_render_resource.find(instance);
            GLTF_CHECK(instance_data != mesh_instance_render_resource.end());

            memcpy(instance_buffer.data.get() + instance_index * sizeof(glm::mat4), &instance_data->second.m_instance_transform, sizeof(glm::mat4)); 
            ++instance_index;
        }
    }
    
    if (!m_instance_buffer_view)
    {
        const RHIBufferDesc instance_buffer_desc = {L"instanceBufferDefaultBuffer", instance_buffer.byteSize, 1, 1, RHIBufferType::Default, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
        m_instance_buffer_view = m_instance_buffer->CreateVertexBufferView(resource_manager.GetDevice(), command_list, instance_buffer_desc, instance_buffer);
    }
    
    
    
    for (const auto& instance : mesh_instance_render_resource)
    {
        const auto& mesh_data = mesh_render_resources.find(instance.second.m_mesh_render_resource);
        if (mesh_data == mesh_render_resources.end())
        {
            // No valid instance data exists..
            continue;
        }
        
        if (!mesh_data->second.mesh->IsVisible())
        {
            continue;
        }
        
        RETURN_IF_FALSE(BeginDrawMesh(resource_manager, mesh_data->second, instance.second))
        
        RHIUtils::Instance().SetVertexBufferView(command_list, 0, *mesh_data->second.mesh_vertex_buffer_view);
        RHIUtils::Instance().SetVertexBufferView(command_list, 1, *m_instance_buffer_view);
        RHIUtils::Instance().SetIndexBufferView(command_list, *mesh_data->second.mesh_index_buffer_view);
        
        RHIUtils::Instance().DrawIndexInstanced(command_list,
            mesh_data->second.mesh_index_count, mesh_instance_infos[instance.second.m_mesh_render_resource].first,
            0, 0,
            mesh_instance_infos[instance.second.m_mesh_render_resource].second);

        RETURN_IF_FALSE(EndDrawMesh(resource_manager, mesh_data->second, instance.second))
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
                m_vertex_input_layouts.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(POSITION), 0, RHIDataFormat::R32G32B32_FLOAT, vertex_layout_offset, 0});
            }
            break;
        case VertexLayoutType::NORMAL:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetRHIDataFormatBitsPerPixel(RHIDataFormat::R32G32B32_FLOAT) / 8));
                m_vertex_input_layouts.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(NORMAL), 0, RHIDataFormat::R32G32B32_FLOAT, vertex_layout_offset, 0});
            }
            break;
        case VertexLayoutType::TANGENT:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetRHIDataFormatBitsPerPixel(RHIDataFormat::R32G32B32A32_FLOAT) / 8));
                m_vertex_input_layouts.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(TANGENT), 0, RHIDataFormat::R32G32B32A32_FLOAT, vertex_layout_offset, 0});
            }
            break;
        case VertexLayoutType::TEXCOORD_0:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetRHIDataFormatBitsPerPixel(RHIDataFormat::R32G32_FLOAT) / 8));
                m_vertex_input_layouts.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(TEXCOORD), 0, RHIDataFormat::R32G32_FLOAT, vertex_layout_offset, 0});
            }
            break;
            // TODO: Handle TEXCOORD_1?
        }

        vertex_layout_offset += vertex_layout.byte_size;   
    }

    m_vertex_input_layouts.push_back({ "INSTANCE_TRANSFORM_MATRIX", 0, RHIDataFormat::R32G32B32A32_FLOAT, 0, 1 });
    m_vertex_input_layouts.push_back({ "INSTANCE_TRANSFORM_MATRIX", 1, RHIDataFormat::R32G32B32A32_FLOAT, 16, 1 });
    m_vertex_input_layouts.push_back({ "INSTANCE_TRANSFORM_MATRIX", 2, RHIDataFormat::R32G32B32A32_FLOAT, 32, 1 });
    m_vertex_input_layouts.push_back({ "INSTANCE_TRANSFORM_MATRIX", 3, RHIDataFormat::R32G32B32A32_FLOAT, 48, 1 });

    return true;
}