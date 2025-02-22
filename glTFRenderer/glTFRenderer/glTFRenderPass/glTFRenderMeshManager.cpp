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
    
    if (!m_mesh_render_resources.contains(mesh_ID))
    {
        m_mesh_render_resources[mesh_ID].mesh = primitive;
        m_mesh_render_resources[mesh_ID].mesh_vertex_buffer = std::make_shared<RHIVertexBuffer>();
        const RHIBufferDesc vertex_buffer_desc =
            {
                L"vertexBufferDefaultBuffer",
                primitive->GetVertexBufferData().byte_size,
                1,
                1,
                RHIBufferType::Default,
                RHIDataFormat::UNKNOWN,
                RHIBufferResourceType::Buffer,
                RHIResourceStateType::STATE_COMMON,
                static_cast<RHIResourceUsageFlags>(RUF_VERTEX_BUFFER | RUF_TRANSFER_DST)
            };
        m_mesh_render_resources[mesh_ID].mesh_vertex_buffer_view = m_mesh_render_resources[mesh_ID].mesh_vertex_buffer->CreateVertexBufferView(device, memory_manager, command_list, vertex_buffer_desc, primitive->GetVertexBufferData());

        m_mesh_render_resources[mesh_ID].mesh_position_only_buffer = std::make_shared<RHIVertexBuffer>();
        const RHIBufferDesc position_only_buffer_desc =
            {
                L"positionOnlyBufferDefaultBuffer",
                primitive->GetPositionOnlyBufferData().byte_size,
                1,
                1,
                RHIBufferType::Default,
                RHIDataFormat::UNKNOWN,
                RHIBufferResourceType::Buffer,
                RHIResourceStateType::STATE_COMMON,
                static_cast<RHIResourceUsageFlags>(RUF_VERTEX_BUFFER | RUF_TRANSFER_DST)
            };
        m_mesh_render_resources[mesh_ID].mesh_position_only_buffer_view = m_mesh_render_resources[mesh_ID].mesh_position_only_buffer->CreateVertexBufferView(device, memory_manager, command_list, position_only_buffer_desc, primitive->GetPositionOnlyBufferData());

        m_mesh_render_resources[mesh_ID].mesh_index_buffer = std::make_shared<RHIIndexBuffer>();
        const RHIBufferDesc index_buffer_desc =
            {
                L"indexBufferDefaultBuffer",
                primitive->GetIndexBufferData().byte_size,
                1,
                1,
                RHIBufferType::Default,
                RHIDataFormat::UNKNOWN,
                RHIBufferResourceType::Buffer,
                RHIResourceStateType::STATE_COMMON,
                static_cast<RHIResourceUsageFlags>(RUF_INDEX_BUFFER | RUF_TRANSFER_DST)
            };
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
        instance_buffer.layout.elements = GetVertexStreamingManager().GetInstanceInputLayout();
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
            RHIBufferResourceType::Buffer,
            RHIResourceStateType::STATE_COMMON,
            static_cast<RHIResourceUsageFlags>(RUF_VERTEX_BUFFER | RUF_TRANSFER_DST)
        };
        
        m_instance_buffer = std::make_shared<RHIVertexBuffer>();
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
        
        auto mega_index_stride_byte_size = GetBytePerPixelByFormat(index_format);
        
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
            RHIBufferResourceType::Buffer,
            RHIResourceStateType::STATE_COMMON,
            static_cast<RHIResourceUsageFlags>(RUF_INDEX_BUFFER | RUF_TRANSFER_DST)
        };

        m_mega_index_buffer = std::make_shared<RHIIndexBuffer>();
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
    
    RHIIndirectArgumentDesc draw_desc;
    draw_desc.type = RHIIndirectArgType::DrawIndexed;
    indirect_argument_descs.push_back(draw_desc);
    
    GLTF_CHECK(!m_indirect_draw_builder);    
    m_indirect_draw_builder = std::make_shared<RHIIndirectDrawBuilder>();
    m_indirect_draw_builder->InitIndirectDrawBuilder(
        resource_manager.GetDevice(),
        resource_manager.GetMemoryManager(),
        indirect_argument_descs,
        sizeof(MeshIndirectDrawCommand),
        m_indirect_arguments.data(),
        m_indirect_arguments.size() * sizeof(MeshIndirectDrawCommand));
    
    return true;
}

RHIIndirectDrawBuilder& glTFRenderMeshManager::GetIndirectDrawBuilder()
{
    return *m_indirect_draw_builder;
}

const RHIIndirectDrawBuilder& glTFRenderMeshManager::GetIndirectDrawBuilder() const
{
    return *m_indirect_draw_builder;
}

bool glTFRenderMeshManager::ResolveVertexInputLayout(const VertexLayoutDeclaration& source_vertex_layout)
{
    return m_vertex_streaming_manager.Init(source_vertex_layout);
}

const RHIVertexStreamingManager& glTFRenderMeshManager::GetVertexStreamingManager() const
{
    return m_vertex_streaming_manager;
}
