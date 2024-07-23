#include "glTFRenderMeshManager.h"

#include "glTFRenderResourceManager.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

bool glTFRenderMeshManager::AddOrUpdatePrimitive(glTFRenderResourceManager& resource_manager,
                                                 const glTFScenePrimitive* primitive)
{
    auto& device = resource_manager.GetDevice();
    auto& command_list = resource_manager.GetCommandListForRecord();
    auto& memory_manager = resource_manager.GetMemoryManager();
    
    const glTFUniqueID mesh_ID = primitive->GetMeshRawDataID();
    const glTFUniqueID instance_ID = primitive->GetID();
    
    if (m_mesh_render_resources.find(mesh_ID) == m_mesh_render_resources.end())
    {
        m_mesh_render_resources[mesh_ID].mesh = primitive;
        m_mesh_render_resources[mesh_ID].mesh_vertex_buffer = RHIResourceFactory::CreateRHIResource<IRHIVertexBuffer>();
        const RHIBufferDesc vertex_buffer_desc = {L"vertexBufferDefaultBuffer", primitive->GetVertexBufferData().byte_size, 1, 1, RHIBufferType::Default, RHIDataFormat::UNKNOWN, RHIBufferResourceType::Buffer};
        m_mesh_render_resources[mesh_ID].mesh_vertex_buffer_view = m_mesh_render_resources[mesh_ID].mesh_vertex_buffer->CreateVertexBufferView(device, memory_manager, command_list, vertex_buffer_desc, primitive->GetVertexBufferData());

        m_mesh_render_resources[mesh_ID].mesh_position_only_buffer = RHIResourceFactory::CreateRHIResource<IRHIVertexBuffer>();
        const RHIBufferDesc position_only_buffer_desc = {L"positionOnlyBufferDefaultBuffer", primitive->GetPositionOnlyBufferData().byte_size, 1, 1, RHIBufferType::Default, RHIDataFormat::UNKNOWN, RHIBufferResourceType::Buffer};
        m_mesh_render_resources[mesh_ID].mesh_position_only_buffer_view = m_mesh_render_resources[mesh_ID].mesh_position_only_buffer->CreateVertexBufferView(device, memory_manager, command_list, position_only_buffer_desc, primitive->GetPositionOnlyBufferData());

        m_mesh_render_resources[mesh_ID].mesh_index_buffer = RHIResourceFactory::CreateRHIResource<IRHIIndexBuffer>();
        const RHIBufferDesc index_buffer_desc = {L"indexBufferDefaultBuffer", primitive->GetIndexBufferData().byte_size, 1, 1, RHIBufferType::Default, RHIDataFormat::UNKNOWN, RHIBufferResourceType::Buffer};
        m_mesh_render_resources[mesh_ID].mesh_index_buffer_view = m_mesh_render_resources[mesh_ID].mesh_index_buffer->CreateIndexBufferView(device, memory_manager, command_list, index_buffer_desc, primitive->GetIndexBufferData());
       
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
    auto& memory_manager = resource_manager.GetMemoryManager();
    
    if (!m_instance_buffer_view)
    {
        VertexBufferData instance_buffer;
        instance_buffer.layout.elements = m_instance_input_layout.m_instance_layout.elements;
        instance_buffer.byte_size = mesh_instance_render_resource.size() * sizeof(MeshInstanceInputData);
        instance_buffer.vertex_count = mesh_instance_render_resource.size();
    
        instance_buffer.data = std::make_unique<char[]>(instance_buffer.byte_size);
        m_instance_data.reserve(mesh_instance_render_resource.size());
    
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

            m_instance_draw_infos[mesh_ID] = {instances.size(), m_instance_data.size()};

            for (size_t instance_index = 0; instance_index < instances.size(); ++instance_index)
            {
                auto instance_data = mesh_instance_render_resource.find(instances[instance_index]);
                GLTF_CHECK(instance_data != mesh_instance_render_resource.end());
            
                m_instance_data.emplace_back( MeshInstanceInputData
                    {
                        instance_data->second.m_instance_transform,
                        instance_data->second.m_instance_material_id,
                        instance_data->second.m_normal_mapping ? 1u : 0u,
                        mesh_ID, 0
                    });
            }
        }

        memcpy(instance_buffer.data.get(), m_instance_data.data(), instance_buffer.byte_size);
    
        const RHIBufferDesc instance_buffer_desc =
        {
            L"instanceBufferDefaultBuffer",
            instance_buffer.byte_size,
            1,
            1,
            RHIBufferType::Default,
            RHIDataFormat::UNKNOWN,
            RHIBufferResourceType::Buffer
        };
        
        m_instance_buffer = RHIResourceFactory::CreateRHIResource<IRHIVertexBuffer>();
        m_instance_buffer_view = m_instance_buffer->CreateVertexBufferView(
            resource_manager.GetDevice(), memory_manager, resource_manager.GetCommandListForRecord(), instance_buffer_desc, instance_buffer);
    }

    std::map<glTFUniqueID, size_t> mesh_start_index_map;
    if (!m_mega_index_buffer_view)
    {
        size_t mega_index_buffer_count = 0;
        size_t mega_index_buffer_byte_size = 0;
        RHIDataFormat index_format {RHIDataFormat::UNKNOWN};
        for (const auto& mesh : mesh_render_resources)
        {
            mesh_start_index_map[mesh.first] = mega_index_buffer_count;
            mega_index_buffer_count += mesh.second.mesh_index_count;
            mega_index_buffer_byte_size += mesh.second.mesh->GetIndexBufferData().byte_size;

            if (index_format == RHIDataFormat::UNKNOWN)
            {
                index_format = mesh.second.mesh->GetIndexBufferData().format;    
            }
            else
            {
                GLTF_CHECK(index_format == mesh.second.mesh->GetIndexBufferData().format);
            }
        }
        
        auto mega_index_stride_byte_size = GetRHIDataFormatBitsPerPixel(index_format) / 8;
        
        IndexBufferData mega_index_buffer_data;
        mega_index_buffer_data.byte_size = mega_index_buffer_byte_size;
        mega_index_buffer_data.data = std::make_unique<char[]>(mega_index_buffer_byte_size);
        mega_index_buffer_data.format = index_format;
        mega_index_buffer_data.index_count = mega_index_buffer_count;
        for (const auto& mesh : mesh_render_resources)
        {
            auto& index_buffer_data = mesh.second.mesh->GetIndexBufferData();
            
            auto start_index_buffer_offset = mesh_start_index_map.find(mesh.first);
            GLTF_CHECK(start_index_buffer_offset != mesh_start_index_map.end());

            auto offset = start_index_buffer_offset->second * mega_index_stride_byte_size;
            memcpy(mega_index_buffer_data.data.get() + offset, index_buffer_data.data.get(), index_buffer_data.byte_size);
        }
        
        RHIBufferDesc mega_index_buffer_desc =
        {
            L"mega_index_buffer",
            mega_index_buffer_byte_size,
            1,
            1,
            RHIBufferType::Default,
            index_format,
            RHIBufferResourceType::Buffer
        };

        m_mega_index_buffer = RHIResourceFactory::CreateRHIResource<IRHIIndexBuffer>();
        m_mega_index_buffer_view = m_mega_index_buffer->CreateIndexBufferView(
            resource_manager.GetDevice(), memory_manager, resource_manager.GetCommandListForRecord(), mega_index_buffer_desc, mega_index_buffer_data);
    }
    
    std::vector<MeshIndirectDrawCommand> m_indirect_arguments; 
    // Gather all mesh for indirect drawing
    for (const auto& instance : m_instance_draw_infos)
    {
        const auto& mesh_data = mesh_render_resources.find(instance.first);
        auto start_index_buffer_offset = mesh_start_index_map.find(instance.first);
        GLTF_CHECK(start_index_buffer_offset != mesh_start_index_map.end());
        
        MeshIndirectDrawCommand command{};
        command.draw_command_argument.IndexCountPerInstance = mesh_data->second.mesh_index_count;
        command.draw_command_argument.InstanceCount = instance.second.first;
        command.draw_command_argument.StartInstanceLocation = instance.second.second;
        command.draw_command_argument.BaseVertexLocation = 0;
        command.draw_command_argument.StartIndexLocation = start_index_buffer_offset->second;

        m_indirect_arguments.push_back(command);
    }

    std::vector<RHIIndirectArgumentDesc> indirect_argument_descs;
    
    /*
    RHIIndirectArgumentDesc vertex_buffer_desc;
    vertex_buffer_desc.type = RHIIndirectArgType::VertexBufferView;
    vertex_buffer_desc.desc.vertex_buffer.slot = 0;
    indirect_argument_descs.push_back(vertex_buffer_desc);

    RHIIndirectArgumentDesc vertex_buffer_for_instancing_desc;
    vertex_buffer_for_instancing_desc.type = RHIIndirectArgType::VertexBufferView;
    vertex_buffer_for_instancing_desc.desc.vertex_buffer.slot = 1;
    indirect_argument_descs.push_back(vertex_buffer_for_instancing_desc);

    RHIIndirectArgumentDesc index_buffer_desc;
    index_buffer_desc.type = RHIIndirectArgType::IndexBufferView;
    indirect_argument_descs.push_back(index_buffer_desc);
*/
    RHIIndirectArgumentDesc draw_desc;
    draw_desc.type = RHIIndirectArgType::DrawIndexed;
    indirect_argument_descs.push_back(draw_desc);
    
    GLTF_CHECK(!m_indirect_draw_builder);    
    m_indirect_draw_builder = RHIResourceFactory::CreateRHIResource<IRHIIndirectDrawBuilder>();
    m_indirect_draw_builder->InitIndirectDrawBuilder(
        resource_manager.GetDevice(),
        resource_manager.GetMemoryManager(),
        indirect_argument_descs,
        sizeof(MeshIndirectDrawCommand),
        m_indirect_arguments.data(),
        m_indirect_arguments.size() * sizeof(MeshIndirectDrawCommand));
    
    return true;
}

IRHIIndirectDrawBuilder& glTFRenderMeshManager::GetIndirectDrawBuilder()
{
    return *m_indirect_draw_builder;
}

const IRHIIndirectDrawBuilder& glTFRenderMeshManager::GetIndirectDrawBuilder() const
{
    return *m_indirect_draw_builder;
}

bool glTFRenderMeshManager::ResolveVertexInputLayout(const VertexLayoutDeclaration& source_vertex_layout)
{
    m_vertex_input_layouts.clear();
    
    unsigned vertex_layout_offset = 0;
    for (const auto& vertex_layout : source_vertex_layout.elements)
    {
        switch (vertex_layout.type)
        {
        case VertexAttributeType::POSITION:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetRHIDataFormatBitsPerPixel(RHIDataFormat::R32G32B32_FLOAT) / 8));
                m_vertex_input_layouts.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(POSITION), 0, RHIDataFormat::R32G32B32_FLOAT, vertex_layout_offset, 0});
            }
            break;
        case VertexAttributeType::NORMAL:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetRHIDataFormatBitsPerPixel(RHIDataFormat::R32G32B32_FLOAT) / 8));
                m_vertex_input_layouts.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(NORMAL), 0, RHIDataFormat::R32G32B32_FLOAT, vertex_layout_offset, 0});
            }
            break;
        case VertexAttributeType::TANGENT:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetRHIDataFormatBitsPerPixel(RHIDataFormat::R32G32B32A32_FLOAT) / 8));
                m_vertex_input_layouts.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(TANGENT), 0, RHIDataFormat::R32G32B32A32_FLOAT, vertex_layout_offset, 0});
            }
            break;
        case VertexAttributeType::TEXCOORD_0:
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
