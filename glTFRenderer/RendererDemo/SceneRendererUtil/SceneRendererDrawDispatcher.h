#pragma once
#include "Renderer.h"
#include "RendererInterface.h"
#include <glm/glm/glm.hpp>

// ----------- must match SceneRendererCommon.hlsl ----------
struct SceneMeshDataOffsetInfo
{
    unsigned material_index;
    unsigned start_vertex_index; // -- vertex info start index
};

struct SceneMeshVertexInfo
{
    float position[4];
    float normal[4];
    float tangent[4];
    float uv[4];
};

struct SceneMeshInstanceRenderResource
{
    glm::mat4 m_instance_transform;
    unsigned m_instance_material_id;
    unsigned m_mesh_id; // -- mesh start info index
    unsigned padding[2];
};
// ----------- must match SceneRendererCommon.hlsl ----------

class RendererSceneMeshDataAccessor : public RendererInterface::RendererSceneMeshDataAccessorBase
{
public:
    RendererSceneMeshDataAccessor(RendererInterface::ResourceOperator& resource_operator);
    
    virtual bool HasMeshData(unsigned mesh_id) const override;
    virtual void AccessMeshData(MeshDataAccessorType type, unsigned mesh_id, void* data, size_t element_size) override;
    virtual void AccessInstanceData(MeshDataAccessorType type, unsigned instance_id, unsigned mesh_id, void* data, size_t element_size) override;

    std::map<unsigned, unsigned> mesh_index_counts;
    std::map<unsigned, RendererInterface::IndexedBufferHandle> mesh_index_buffers;
    
    // mesh data
    std::vector<SceneMeshDataOffsetInfo> start_offset_infos;
    std::vector<SceneMeshVertexInfo> mesh_vertex_infos;

    // instance data
    std::vector<SceneMeshInstanceRenderResource> instance_render_resources;

    // draw data
    std::vector<RendererInterface::RenderExecuteCommand> execute_commands;

    RendererInterface::ResourceOperator& m_resource_operator;
};

class SceneRendererDrawDispatcher
{
public:
    SceneRendererDrawDispatcher(RendererInterface::ResourceOperator& resource_operator, const std::string& scene_file);
    bool BindDrawCommands(RendererInterface::RenderPassDrawDesc& out_draw_desc);
    
protected:
    RendererSceneMeshDataAccessor m_mesh_data_accessor;
    std::unique_ptr<RendererInterface::RendererSceneResourceManager> m_resource_manager;

    RendererInterface::BufferDesc vertex_info_buffer_desc{};
    RendererInterface::BufferDesc start_offset_info_buffer_desc{};
    RendererInterface::BufferDesc instance_render_resources_buffer_desc{};
    
    RendererInterface::BufferHandle m_mesh_buffer_vertex_info_handle {NULL_HANDLE};
    RendererInterface::BufferHandle m_mesh_buffer_start_info_handle {NULL_HANDLE};
    RendererInterface::BufferHandle m_mesh_buffer_instance_info_handle {NULL_HANDLE};

    std::vector<RendererInterface::RenderExecuteCommand> m_draw_commands;
};
