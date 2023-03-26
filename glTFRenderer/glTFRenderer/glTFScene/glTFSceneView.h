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
    void TraversePrimitiveWithinView(const std::function<bool(const glTFSceneNode& primitive)>& visitor);

    glm::mat4 GetViewProjectionMatrix() const;
    
private:
    const glTFSceneGraph& m_sceneGraph;

    std::vector<const glTFCamera*> m_cameras;
};
