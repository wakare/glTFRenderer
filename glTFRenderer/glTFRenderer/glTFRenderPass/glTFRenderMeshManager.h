#pragma once
#include "glTFMeshRenderResource.h"
#include "glTFRHI/IRHIIndirectDrawBuilder.h"
#include "glTFRHI/RHIVertexStreamingManager.h"

class glTFRenderResourceManager;

struct glTFMeshInstanceRenderResource
{
    glm::mat4 m_instance_transform;
    glTFUniqueID m_mesh_render_resource;
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
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(glTFRenderMeshManager)
    
    bool AddOrUpdatePrimitive(glTFRenderResourceManager& resource_manager, const glTFScenePrimitive* primitive);
    bool BuildMeshRenderResource(glTFRenderResourceManager& resource_manager);

    const std::map<glTFUniqueID, glTFMeshInstanceRenderResource>& GetMeshInstanceRenderResource() const {return m_mesh_instances; }
    const std::map<glTFUniqueID, glTFMeshRenderResource>& GetMeshRenderResources() const {return m_mesh_render_resources; }

    IRHIIndirectDrawBuilder& GetIndirectDrawBuilder();
    const IRHIIndirectDrawBuilder& GetIndirectDrawBuilder() const;
    
    const std::vector<MeshInstanceInputData>& GetInstanceBufferData() const {return m_instance_data; }
    
    std::shared_ptr<IRHIVertexBuffer> GetInstanceBuffer() const {return m_instance_buffer; }
    std::shared_ptr<IRHIVertexBufferView> GetInstanceBufferView() const {return m_instance_buffer_view; }
    const std::map<glTFUniqueID, std::pair<unsigned, unsigned>>& GetInstanceDrawInfo() const {return m_instance_draw_infos; }
    
    bool ResolveVertexInputLayout(const VertexLayoutDeclaration& source_vertex_layout);

    std::shared_ptr<IRHIIndexBuffer> GetMegaIndexBuffer() const {return m_mega_index_buffer; }
    std::shared_ptr<IRHIIndexBufferView> GetMegaIndexBufferView() const {return m_mega_index_buffer_view; }

    const RHIVertexStreamingManager& GetVertexStreamingManager() const;

protected:
    std::map<glTFUniqueID, glTFMeshInstanceRenderResource> m_mesh_instances;
    std::map<glTFUniqueID, glTFMeshRenderResource> m_mesh_render_resources;

    std::shared_ptr<IRHIIndirectDrawBuilder> m_indirect_draw_builder;
    
    // mesh id -- <instance count, instance start offset>
    std::map<glTFUniqueID, std::pair<unsigned, unsigned>> m_instance_draw_infos;

    std::vector<MeshInstanceInputData> m_instance_data;
    
    std::shared_ptr<IRHIVertexBuffer> m_instance_buffer;
    std::shared_ptr<IRHIVertexBufferView> m_instance_buffer_view;

    std::shared_ptr<IRHIIndexBuffer> m_mega_index_buffer;
    std::shared_ptr<IRHIIndexBufferView> m_mega_index_buffer_view;

    RHIVertexStreamingManager m_vertex_streaming_manager;
};

