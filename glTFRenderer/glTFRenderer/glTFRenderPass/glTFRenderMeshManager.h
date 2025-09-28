#pragma once
#include <glm.hpp>

#include "glTFMeshRenderResource.h"
#include "RHIIndirectDrawBuilder.h"
#include "RHIVertexStreamingManager.h"

class glTFRenderResourceManager;

struct glTFMeshInstanceRenderResource
{
    glm::mat4 m_instance_transform;
    RendererUniqueObjectID m_mesh_render_resource;
    unsigned m_instance_material_id;
    bool m_normal_mapping;
};

ALIGN_FOR_CBV_STRUCT struct MeshInstanceInputData
{
    inline static std::string Name = "MESH_INSTANCE_INPUT_DATA_REGISTER_SRV_INDEX";
    
    glm::mat4 instance_transform;
    unsigned instance_material_id;
    unsigned normal_mapping;
    unsigned mesh_id;
    unsigned padding;
};

class glTFRenderMeshManager
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(glTFRenderMeshManager)
    
    bool AddOrUpdatePrimitive(glTFRenderResourceManager& resource_manager, const glTFScenePrimitive* primitive);
    bool BuildMeshRenderResource(glTFRenderResourceManager& resource_manager);

    const std::map<RendererUniqueObjectID, glTFMeshInstanceRenderResource>& GetMeshInstanceRenderResource() const {return m_mesh_instances; }
    const std::map<RendererUniqueObjectID, glTFMeshRenderResource>& GetMeshRenderResources() const {return m_mesh_render_resources; }

    RHIIndirectDrawBuilder& GetIndirectDrawBuilder();
    const RHIIndirectDrawBuilder& GetIndirectDrawBuilder() const;
    
    const std::vector<MeshInstanceInputData>& GetInstanceBufferData() const {return m_instance_data; }
    
    std::shared_ptr<RHIVertexBuffer> GetInstanceBuffer() const {return m_instance_buffer; }
    std::shared_ptr<IRHIVertexBufferView> GetInstanceBufferView() const {return m_instance_buffer_view; }
    const std::map<RendererUniqueObjectID, std::pair<unsigned, unsigned>>& GetInstanceDrawInfo() const {return m_instance_draw_infos; }
    
    bool ResolveVertexInputLayout(const VertexLayoutDeclaration& source_vertex_layout);

    std::shared_ptr<RHIIndexBuffer> GetMegaIndexBuffer() const {return m_mega_index_buffer; }
    std::shared_ptr<IRHIIndexBufferView> GetMegaIndexBufferView() const {return m_mega_index_buffer_view; }

    const RHIVertexStreamingManager& GetVertexStreamingManager() const;

protected:
    std::map<RendererUniqueObjectID, glTFMeshInstanceRenderResource> m_mesh_instances;
    std::map<RendererUniqueObjectID, glTFMeshRenderResource> m_mesh_render_resources;

    std::shared_ptr<RHIIndirectDrawBuilder> m_indirect_draw_builder;
    
    // mesh id -- <instance count, instance start offset>
    std::map<RendererUniqueObjectID, std::pair<unsigned, unsigned>> m_instance_draw_infos;

    std::vector<MeshInstanceInputData> m_instance_data;
    
    std::shared_ptr<RHIVertexBuffer> m_instance_buffer;
    std::shared_ptr<IRHIVertexBufferView> m_instance_buffer_view;

    std::shared_ptr<RHIIndexBuffer> m_mega_index_buffer;
    std::shared_ptr<IRHIIndexBufferView> m_mega_index_buffer_view;

    RHIVertexStreamingManager m_vertex_streaming_manager;
};

