#pragma once
#include <memory>

#include "glTFSceneGraph.h"
#include "glTFScenePrimitive.h"
#include "glTFCamera.h"

// Handle render scene graph with specific camera
class glTFSceneView
{
public:
    glTFSceneView(const glTFSceneGraph& graph);

    // Only traverse necessary primitives
    void TraverseSceneObjectWithinView(const std::function<bool(const glTFSceneNode& primitive)>& visitor);

    glm::mat4 GetViewProjectionMatrix() const;

    void ApplyMovement(const glm::fvec3& translation, const glm::fvec3& rotation);
    
private:
    const glTFSceneGraph& m_sceneGraph;

    std::vector<glTFCamera*> m_cameras;
};
