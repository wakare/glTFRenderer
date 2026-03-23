#pragma once
#include "Renderer.h"
#include "RendererInterface.h"
#include <glm/glm/glm.hpp>

#include "RendererSceneAABB.h"
#include "RendererModule/RendererModuleMaterial.h"

class RendererModuleLightmap;

// ----------- must match SceneRendererCommon.hlsl ----------
struct SceneMeshDataOffsetInfo
{
    unsigned material_index;
    unsigned start_vertex_index; // -- vertex info start index
    unsigned primitive_hash;
    unsigned padding{0};
};

struct SceneMeshVertexInfo
{
    float position[4];
    float normal[4];
    float tangent[4];
    float uv[4]; // xy = uv0, zw = uv1
};

struct SceneMeshInstanceRenderResource
{
    glm::mat4 m_instance_transform;
    unsigned m_instance_material_id;
    unsigned m_mesh_id; // -- mesh start info index
    unsigned m_stable_node_key;
    unsigned m_lightmap_binding_index;
};
// ----------- must match SceneRendererCommon.hlsl ----------

class RendererSceneMeshDataAccessor : public RendererInterface::RendererSceneMeshDataAccessorBase
{
public:
    RendererSceneMeshDataAccessor(RendererInterface::ResourceOperator& resource_operator,
                                  RendererModuleMaterial& material_module,
                                  RendererModuleLightmap* lightmap_module);
    
    virtual bool HasMeshData(unsigned mesh_id) const override;
    virtual void AccessMeshData(MeshDataAccessorType type, unsigned mesh_id, void* data, size_t element_size) override;
    virtual void AccessInstanceData(MeshDataAccessorType type, unsigned instance_id, unsigned mesh_id, void* data, size_t element_size) override;

    virtual void AccessMaterialData(const MaterialBase& material, unsigned mesh_id) override;

    RendererInterface::ResourceOperator& m_resource_operator;
    RendererModuleMaterial& m_material_module;
    RendererModuleLightmap* m_lightmap_module{nullptr};
    
    std::map<unsigned, unsigned> mesh_index_counts;
    std::map<unsigned, RendererInterface::IndexedBufferHandle> mesh_index_buffers;
    std::map<unsigned, unsigned> instance_stable_node_keys;
    
    // mesh data
    std::vector<SceneMeshDataOffsetInfo> start_offset_infos;
    std::vector<SceneMeshVertexInfo> mesh_vertex_infos;

    // instance data
    std::vector<SceneMeshInstanceRenderResource> instance_render_resources;

    // draw data
    std::vector<RendererInterface::RenderExecuteCommand> execute_commands;
};

class RendererModuleSceneMesh : public RendererInterface::RendererModuleBase
{
public:
    RendererModuleSceneMesh(RendererInterface::ResourceOperator& resource_operator,
                            const std::string& scene_file,
                            const std::shared_ptr<RendererModuleLightmap>& lightmap_module = {});
    virtual bool FinalizeModule(RendererInterface::ResourceOperator& resource_operator) override;
    virtual bool BindDrawCommands(RendererInterface::RenderPassDrawDesc& out_draw_desc) override;
    virtual bool Tick(RendererInterface::ResourceOperator&, unsigned long long interval) override;
    RendererSceneAABB GetSceneBounds() const;
    
protected:
    std::unique_ptr<RendererInterface::RendererSceneResourceManager> m_resource_manager;

    RendererInterface::BufferDesc vertex_info_buffer_desc{};
    RendererInterface::BufferDesc start_offset_info_buffer_desc{};
    RendererInterface::BufferDesc instance_render_resources_buffer_desc{};
    
    RendererInterface::BufferHandle m_mesh_buffer_vertex_info_handle {NULL_HANDLE};
    RendererInterface::BufferHandle m_mesh_buffer_start_info_handle {NULL_HANDLE};
    RendererInterface::BufferHandle m_mesh_buffer_instance_info_handle {NULL_HANDLE};

    std::vector<RendererInterface::RenderExecuteCommand> m_draw_commands;
    std::unique_ptr<RendererModuleMaterial> m_module_material;
    std::shared_ptr<RendererModuleLightmap> m_module_lightmap;
    RendererSceneMeshDataAccessor m_mesh_data_accessor;
};
