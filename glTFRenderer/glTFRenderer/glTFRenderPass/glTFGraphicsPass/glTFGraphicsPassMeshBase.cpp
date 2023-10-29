#include "glTFGraphicsPassMeshBase.h"

#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneMesh.h"
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

    RETURN_IF_FALSE(InitInstanceBuffer(resource_manager))
    const auto& mesh_render_resources = resource_manager.GetMeshManager().GetMeshRenderResources();
    
    for (const auto& instance : m_instance_draw_infos)
    {
        const auto& mesh_data = mesh_render_resources.find(instance.first);
        if (mesh_data == mesh_render_resources.end())
        {
            // No valid instance data exists..
            continue;
        }
        
        if (!mesh_data->second.mesh->IsVisible())
        {
            continue;
        }
        
        RETURN_IF_FALSE(BeginDrawMesh(resource_manager, mesh_data->second, instance.first))
        
        RHIUtils::Instance().SetVertexBufferView(command_list, 0, *mesh_data->second.mesh_vertex_buffer_view);
        RHIUtils::Instance().SetVertexBufferView(command_list, 1, *m_instance_buffer_view);
        RHIUtils::Instance().SetIndexBufferView(command_list, *mesh_data->second.mesh_index_buffer_view);
        
        RHIUtils::Instance().DrawIndexInstanced(command_list,
            mesh_data->second.mesh_index_count, instance.second.first,
            0, 0,
            instance.second.second);

        //RETURN_IF_FALSE(EndDrawMesh(resource_manager, mesh_data->second, instance.second))
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

bool glTFGraphicsPassMeshBase::BeginDrawMesh(glTFRenderResourceManager& resource_manager, const glTFMeshRenderResource& mesh_data, unsigned mesh_index)
{
    // Upload constant buffer
    ConstantBufferSceneMesh temp_mesh_data =
    {
        glm::mat4(1.0f),
        glm::mat4(1.0f),
        mesh_data.material_id, mesh_data.using_normal_mapping
    };
        
    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceSceneMesh>()->UploadAndApplyDataWithIndex(resource_manager, mesh_index, temp_mesh_data, true))
    
    return true;   
}

bool glTFGraphicsPassMeshBase::EndDrawMesh(glTFRenderResourceManager& resource_manager, const glTFMeshRenderResource& mesh_data, unsigned mesh_index)
{
    return true;
}

bool glTFGraphicsPassMeshBase::InitInstanceBuffer(glTFRenderResourceManager& resource_manager)
{
    if (m_instance_buffer_view)
    {
        return true;
    }
    
    // Render meshes
    const auto& mesh_manager = resource_manager.GetMeshManager();
    
    const auto& mesh_render_resources = mesh_manager.GetMeshRenderResources();
    const auto& mesh_instance_render_resource = mesh_manager.GetMeshInstanceRenderResource();
    
    VertexBufferData instance_buffer;
    instance_buffer.layout.elements = m_instance_input_layout.m_instance_layout.elements;
    instance_buffer.byteSize = mesh_instance_render_resource.size() * sizeof(MeshInstanceInputData);
    instance_buffer.vertex_count = mesh_instance_render_resource.size();
    
    instance_buffer.data = std::make_unique<char[]>(instance_buffer.byteSize);
    std::vector<MeshInstanceInputData> instance_datas; instance_datas.reserve(mesh_instance_render_resource.size());
    
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

        m_instance_draw_infos[mesh_ID] = {instances.size(), instance_datas.size()};
        
        for (size_t instance_index = 0; instance_index < instances.size(); ++instance_index)
        {
            auto instance_data = mesh_instance_render_resource.find(instances[instance_index]);
            GLTF_CHECK(instance_data != mesh_instance_render_resource.end());
            
            instance_datas.emplace_back( MeshInstanceInputData
                {instance_data->second.m_instance_transform, instance_data->second.m_instance_material_id});
        }
    }

    memcpy(instance_buffer.data.get(), instance_datas.data(), instance_buffer.byteSize);
    
    const RHIBufferDesc instance_buffer_desc = {L"instanceBufferDefaultBuffer", instance_buffer.byteSize, 1, 1, RHIBufferType::Default, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
    m_instance_buffer = RHIResourceFactory::CreateRHIResource<IRHIVertexBuffer>();
    m_instance_buffer_view = m_instance_buffer->CreateVertexBufferView(resource_manager.GetDevice(), resource_manager.GetCommandListForRecord(), instance_buffer_desc, instance_buffer);
    
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

    GLTF_CHECK(m_instance_input_layout.ResolveInputInstanceLayout(m_vertex_input_layouts));
    
    return true;
}