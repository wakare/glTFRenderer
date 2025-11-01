#include "RendererModuleSceneMesh.h"

#include "RendererCommon.h"
#include <glm/glm/gtc/type_ptr.hpp>

#include "RendererSceneCommon.h"

RendererSceneMeshDataAccessor::RendererSceneMeshDataAccessor(RendererInterface::ResourceOperator& resource_operator, RendererModuleMaterial& material_module)
    : m_resource_operator(resource_operator)
    , m_material_module(material_module)
{
    
}

bool RendererSceneMeshDataAccessor::HasMeshData(unsigned mesh_id) const
{
    return mesh_index_counts.contains(mesh_id);
}

void RendererSceneMeshDataAccessor::AccessMeshData(MeshDataAccessorType type, unsigned mesh_id, void* data,
                                                   size_t element_size)
{    
    if (start_offset_infos.size() < (mesh_id + 1))
    {
        SceneMeshDataOffsetInfo mesh_data_offset_info{};
        mesh_data_offset_info.start_vertex_index = mesh_vertex_infos.size();
        mesh_data_offset_info.material_index = 0;

        start_offset_infos.resize(mesh_id + 1);
        start_offset_infos[mesh_id] = mesh_data_offset_info;
        mesh_vertex_infos.resize(mesh_vertex_infos.size() + element_size);
    }

    auto vertex_offset = start_offset_infos[mesh_id].start_vertex_index;
    const float* float_data = static_cast<const float*>(data);
    switch (type)
    {
    case MeshDataAccessorType::VERTEX_POSITION_FLOAT3:
        for (size_t i = 0; i < element_size; i++)
        {
            memcpy(mesh_vertex_infos[vertex_offset + i].position, float_data + i * 3, 3 * sizeof(float));
        }
        break;
    case MeshDataAccessorType::VERTEX_NORMAL_FLOAT3:
        for (size_t i = 0; i < element_size; i++)
        {
            memcpy(mesh_vertex_infos[vertex_offset + i].normal, float_data + i * 3, 3 * sizeof(float));
        }
        break;
    case MeshDataAccessorType::VERTEX_TANGENT_FLOAT4:
        for (size_t i = 0; i < element_size; i++)
        {
            memcpy(mesh_vertex_infos[vertex_offset + i].tangent, float_data + i * 4, 4 * sizeof(float));
        }
        break;
    case MeshDataAccessorType::VERTEX_TEXCOORD0_FLOAT2:
        for (size_t i = 0; i < element_size; i++)
        {
            memcpy(mesh_vertex_infos[vertex_offset + i].uv, float_data + i * 2, 2 * sizeof(float));
        }
        break;
    case MeshDataAccessorType::INDEX_INT:
        {            
            mesh_index_counts[mesh_id] = element_size;
            RendererInterface::BufferDesc index_buffer_desc{};
            index_buffer_desc.type = RendererInterface::DEFAULT;
            index_buffer_desc.size = element_size * sizeof(uint32_t);
            index_buffer_desc.name = "IndexBuffer_R32";
            index_buffer_desc.usage = RendererInterface::USAGE_INDEX_BUFFER_R32;
            index_buffer_desc.data = data;
            auto index_buffer_handle = m_resource_operator.CreateIndexedBuffer(index_buffer_desc);
            mesh_index_buffers[mesh_id] = index_buffer_handle;
        }
        break;
    case MeshDataAccessorType::INDEX_HALF:
        {
            mesh_index_counts[mesh_id] = element_size;
            RendererInterface::BufferDesc index_buffer_desc{};
            index_buffer_desc.type = RendererInterface::DEFAULT;
            index_buffer_desc.size = element_size * sizeof(uint16_t);
            index_buffer_desc.name = "IndexBuffer_R16";
            index_buffer_desc.usage = RendererInterface::USAGE_INDEX_BUFFER_R16;
            index_buffer_desc.data = data;
            auto index_buffer_handle = m_resource_operator.CreateIndexedBuffer(index_buffer_desc);
            mesh_index_buffers[mesh_id] = index_buffer_handle;
        }
        break;
    }
}

void RendererSceneMeshDataAccessor::AccessInstanceData(MeshDataAccessorType type, unsigned instance_id,
    unsigned mesh_id, void* data, size_t element_size)
{
    RendererInterface::RenderExecuteCommand execute_command;
    execute_command.type = RendererInterface::ExecuteCommandType::DRAW_INDEXED_INSTANCING_COMMAND;
    execute_command.parameter.draw_indexed_instance_command_parameter.index_count_per_instance = mesh_index_counts.at(mesh_id); 
    execute_command.parameter.draw_indexed_instance_command_parameter.instance_count = 1;
    execute_command.parameter.draw_indexed_instance_command_parameter.start_index_location = 0;
    execute_command.parameter.draw_indexed_instance_command_parameter.start_vertex_location = 0;
    execute_command.parameter.draw_indexed_instance_command_parameter.start_instance_location = instance_render_resources.size();
    execute_command.input_buffer.index_buffer_handle = mesh_index_buffers[mesh_id];
    
    execute_commands.push_back(execute_command);

    SceneMeshInstanceRenderResource instance_render_resource{};
    instance_render_resource.m_instance_material_id = 0;
    instance_render_resource.m_mesh_id = mesh_id;
    
    float* float_data = static_cast<float*>(data);
    instance_render_resource.m_instance_transform = glm::transpose(glm::make_mat4(float_data));

    instance_render_resources.push_back(instance_render_resource);
}

void RendererSceneMeshDataAccessor::AccessMaterialData(const MaterialBase& material, unsigned mesh_id)
{
    start_offset_infos[mesh_id].material_index = material.GetID();
    m_material_module.AddMaterial(material);   
}

RendererModuleSceneMesh::RendererModuleSceneMesh(RendererInterface::ResourceOperator& resource_operator,
                                                                 const std::string& scene_file)
    : m_resource_manager(std::make_unique<RendererInterface::RendererSceneResourceManager>(resource_operator, RendererInterface::RenderSceneDesc{scene_file}))
    , m_module_material( std::make_unique<RendererModuleMaterial>(resource_operator))
    , m_mesh_data_accessor(resource_operator, *m_module_material)
{
    m_resource_manager->AccessSceneData(m_mesh_data_accessor);

    // build mesh draw buffers
    vertex_info_buffer_desc.type = RendererInterface::DEFAULT;
    vertex_info_buffer_desc.name = "mesh_vertex_info";
    vertex_info_buffer_desc.usage = RendererInterface::USAGE_SRV;
    vertex_info_buffer_desc.size = sizeof(SceneMeshVertexInfo) * m_mesh_data_accessor.mesh_vertex_infos.size();
    vertex_info_buffer_desc.data = m_mesh_data_accessor.mesh_vertex_infos.data();
    m_mesh_buffer_vertex_info_handle = resource_operator.CreateBuffer(vertex_info_buffer_desc);

    start_offset_info_buffer_desc.type = RendererInterface::DEFAULT;
    start_offset_info_buffer_desc.name = "mesh_start_info";
    start_offset_info_buffer_desc.usage = RendererInterface::USAGE_SRV;
    start_offset_info_buffer_desc.size = sizeof(SceneMeshDataOffsetInfo) * m_mesh_data_accessor.start_offset_infos.size();
    start_offset_info_buffer_desc.data = m_mesh_data_accessor.start_offset_infos.data();
    m_mesh_buffer_start_info_handle = resource_operator.CreateBuffer(start_offset_info_buffer_desc);

    instance_render_resources_buffer_desc.type = RendererInterface::DEFAULT;
    instance_render_resources_buffer_desc.name = "mesh_instance_input_data";
    instance_render_resources_buffer_desc.usage = RendererInterface::USAGE_SRV;
    instance_render_resources_buffer_desc.size = sizeof(SceneMeshInstanceRenderResource) * m_mesh_data_accessor.instance_render_resources.size();
    instance_render_resources_buffer_desc.data = m_mesh_data_accessor.instance_render_resources.data();
    m_mesh_buffer_instance_info_handle = resource_operator.CreateBuffer(instance_render_resources_buffer_desc);

    m_draw_commands = m_mesh_data_accessor.execute_commands;

    m_module_material->FinalizeModule(resource_operator);
}

bool RendererModuleSceneMesh::FinalizeModule(RendererInterface::ResourceOperator& resource_operator)
{
    RETURN_IF_FALSE(m_module_material->FinalizeModule(resource_operator))
    
    return true;
}

bool RendererModuleSceneMesh::BindDrawCommands(RendererInterface::RenderPassDrawDesc& out_draw_desc)
{
    RETURN_IF_FALSE(m_module_material->BindDrawCommands(out_draw_desc))
    
    out_draw_desc.execute_commands = m_draw_commands;

    RendererInterface::BufferBindingDesc vertex_info_buffer_binding_desc{};
    vertex_info_buffer_binding_desc.buffer_handle = m_mesh_buffer_vertex_info_handle;
    vertex_info_buffer_binding_desc.binding_type = RendererInterface::BufferBindingDesc::SRV;
    vertex_info_buffer_binding_desc.stride = sizeof(SceneMeshVertexInfo);
    vertex_info_buffer_binding_desc.count = vertex_info_buffer_desc.size / vertex_info_buffer_binding_desc.stride;
    vertex_info_buffer_binding_desc.is_structured_buffer = true;
    out_draw_desc.buffer_resources[vertex_info_buffer_desc.name] = vertex_info_buffer_binding_desc;

    RendererInterface::BufferBindingDesc start_offset_info_buffer_binding_desc{};
    start_offset_info_buffer_binding_desc.buffer_handle = m_mesh_buffer_start_info_handle;
    start_offset_info_buffer_binding_desc.binding_type = RendererInterface::BufferBindingDesc::SRV;
    start_offset_info_buffer_binding_desc.stride = sizeof(SceneMeshDataOffsetInfo);
    start_offset_info_buffer_binding_desc.count = start_offset_info_buffer_desc.size / start_offset_info_buffer_binding_desc.stride;
    start_offset_info_buffer_binding_desc.is_structured_buffer = true;
    out_draw_desc.buffer_resources[start_offset_info_buffer_desc.name] = start_offset_info_buffer_binding_desc;

    RendererInterface::BufferBindingDesc instance_render_resources_buffer_binding_desc{};
    instance_render_resources_buffer_binding_desc.buffer_handle = m_mesh_buffer_instance_info_handle;
    instance_render_resources_buffer_binding_desc.binding_type = RendererInterface::BufferBindingDesc::SRV;
    instance_render_resources_buffer_binding_desc.stride = sizeof(SceneMeshInstanceRenderResource);
    instance_render_resources_buffer_binding_desc.count = instance_render_resources_buffer_desc.size / instance_render_resources_buffer_binding_desc.stride;
    instance_render_resources_buffer_binding_desc.is_structured_buffer = true;
    out_draw_desc.buffer_resources[instance_render_resources_buffer_desc.name] = instance_render_resources_buffer_binding_desc;

    return true;
}

bool RendererModuleSceneMesh::Tick(RendererInterface::ResourceOperator& resource_operator,
    unsigned long long interval)
{
    RETURN_IF_FALSE(RendererModuleBase::Tick(resource_operator, interval))

    RETURN_IF_FALSE(m_module_material->Tick(resource_operator, interval))
    
    return true;
}

RendererSceneAABB RendererModuleSceneMesh::GetSceneBounds() const
{
    return m_resource_manager->GetSceneBounds();
}
