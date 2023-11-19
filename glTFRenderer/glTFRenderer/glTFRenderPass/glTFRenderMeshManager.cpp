#include "glTFRenderMeshManager.h"

#include "glTFRenderResourceManager.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

glTFRenderMeshManager::glTFRenderMeshManager()
    : m_command_signature_desc()
{
    RHIIndirectArgumentDesc vertex_buffer_desc;
    
    vertex_buffer_desc.type = RHIIndirectArgType::VertexBufferView;
    vertex_buffer_desc.desc.vertex_buffer.slot = 0;

    RHIIndirectArgumentDesc vertex_buffer_for_instancing_desc;
    
    vertex_buffer_for_instancing_desc.type = RHIIndirectArgType::VertexBufferView;
    vertex_buffer_for_instancing_desc.desc.vertex_buffer.slot = 1;

    RHIIndirectArgumentDesc index_buffer_desc;
    index_buffer_desc.type = RHIIndirectArgType::IndexBufferView;

    RHIIndirectArgumentDesc draw_desc;
    draw_desc.type = RHIIndirectArgType::DrawIndexed;
    
    m_command_signature_desc.m_indirect_arguments.push_back(vertex_buffer_desc);
    m_command_signature_desc.m_indirect_arguments.push_back(vertex_buffer_for_instancing_desc);
    m_command_signature_desc.m_indirect_arguments.push_back(index_buffer_desc);
    m_command_signature_desc.m_indirect_arguments.push_back(draw_desc);
    m_command_signature_desc.stride = sizeof(MeshIndirectDrawCommand);
}

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

bool glTFRenderMeshManager::BuildMeshRenderResource(glTFRenderResourceManager& resource_manager)
{
    const auto& mesh_render_resources = resource_manager.GetMeshManager().GetMeshRenderResources();
    const auto& mesh_instance_render_resource = resource_manager.GetMeshManager().GetMeshInstanceRenderResource();
    
    if (!m_instance_buffer_view)
    {
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
                    {
                        instance_data->second.m_instance_transform,
                        instance_data->second.m_instance_material_id,
                        instance_data->second.m_normal_mapping ? 1u : 0u,
                        mesh_ID, 0
                    });
            }
        }

        memcpy(instance_buffer.data.get(), instance_datas.data(), instance_buffer.byteSize);
    
        const RHIBufferDesc instance_buffer_desc = {L"instanceBufferDefaultBuffer", instance_buffer.byteSize, 1, 1, RHIBufferType::Default, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
        m_instance_buffer = RHIResourceFactory::CreateRHIResource<IRHIVertexBuffer>();
        m_instance_buffer_view = m_instance_buffer->CreateVertexBufferView(resource_manager.GetDevice(), resource_manager.GetCommandListForRecord(), instance_buffer_desc, instance_buffer);
    }
    
    m_indirect_arguments.clear();
    // Gather all mesh for indirect drawing
    for (const auto& instance : m_instance_draw_infos)
    {
        const auto& mesh_data = mesh_render_resources.find(instance.first);
            
        MeshIndirectDrawCommand command
        (
            *mesh_data->second.mesh_vertex_buffer_view,
            *m_instance_buffer_view,
            *mesh_data->second.mesh_index_buffer_view
        );
        command.draw_command_argument.IndexCountPerInstance = mesh_data->second.mesh_index_count;
        command.draw_command_argument.InstanceCount = instance.second.first;
        command.draw_command_argument.StartInstanceLocation = instance.second.second;
        command.draw_command_argument.BaseVertexLocation = 0;
        command.draw_command_argument.StartIndexLocation = 0;

        m_indirect_arguments.push_back(command);
    }

    m_indirect_argument_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    const RHIBufferDesc indirect_arguments_buffer_desc = {L"indirect_arguments_buffer", 1024ull * 64,
    1, 1, RHIBufferType::Upload, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
        
    RETURN_IF_FALSE(m_indirect_argument_buffer->InitGPUBuffer(resource_manager.GetDevice(), indirect_arguments_buffer_desc ))
    RETURN_IF_FALSE(m_indirect_argument_buffer->UploadBufferFromCPU(m_indirect_arguments.data(), 0, m_indirect_arguments.size() * sizeof(MeshIndirectDrawCommand)))
    
    return true;
}

bool glTFRenderMeshManager::ResolveVertexInputLayout(const VertexLayoutDeclaration& source_vertex_layout)
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
