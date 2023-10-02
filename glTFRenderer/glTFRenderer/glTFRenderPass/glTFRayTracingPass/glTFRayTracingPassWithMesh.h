#pragma once
#include "glTFRayTracingPassBase.h"
#include "glTFRenderPass/glTFRenderPassMeshResource.h"

class glTFRayTracingPassWithMesh : public glTFRayTracingPassBase
{
public:
    bool AddOrUpdatePrimitiveToMeshPass(glTFRenderResourceManager& resource_manager, const glTFScenePrimitive& primitive);
    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resource_manager, const glTFSceneObjectBase& object) override;
    
protected:
    std::map<glTFUniqueID, glTFRenderPassMeshResource> m_meshes;
};
