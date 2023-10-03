#pragma once
#include "glTFRenderPassMeshResource.h"

class glTFRenderResourceManager;

class glTFRenderMeshManager
{
public:
    bool AddOrUpdatePrimitive(glTFRenderResourceManager& resource_manager, const glTFScenePrimitive& primitive);
    const std::map<glTFUniqueID, glTFRenderPassMeshResource>& GetMeshes() const {return m_meshes; }
    
protected:
    std::map<glTFUniqueID, glTFRenderPassMeshResource> m_meshes;
};
