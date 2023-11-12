#pragma once
#include "glTFMeshRenderResource.h"

class glTFRenderResourceManager;

struct glTFMeshInstanceRenderResource
{
    glm::mat4 m_instance_transform;
    glTFUniqueID m_mesh_render_resource;
    unsigned m_instance_material_id;
    bool m_normal_mapping;
};

class glTFRenderMeshManager
{
public:
    bool AddOrUpdatePrimitive(glTFRenderResourceManager& resource_manager, const glTFScenePrimitive* primitive);
    const std::map<glTFUniqueID, glTFMeshInstanceRenderResource>& GetMeshInstanceRenderResource() const {return m_mesh_instances; }
    const std::map<glTFUniqueID, glTFMeshRenderResource>& GetMeshRenderResources() const {return m_mesh_render_resources; }
    
protected:
    std::map<glTFUniqueID, glTFMeshInstanceRenderResource> m_mesh_instances;
    std::map<glTFUniqueID, glTFMeshRenderResource> m_mesh_render_resources;
};
