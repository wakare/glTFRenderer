#pragma once
#include <memory>

#include "glTFSceneGraph.h"
#include "glTFScenePrimitive.h"
#include "glTFCamera.h"

class glTFInputManager;

// Handle render scene graph with specific camera
class glTFSceneView
{
public:
    glTFSceneView(const glTFSceneGraph& graph);

    // Only traverse necessary primitives
    void TraverseSceneObjectWithinView(const std::function<bool(const glTFSceneNode& primitive)>& visitor);

    glm::mat4 GetViewProjectionMatrix() const;
    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix() const;

    void ApplyInput(glTFInputManager& input_manager, size_t delta_time_ms) const;
    
private:
    const glTFSceneGraph& m_scene_graph;

    std::vector<glTFCamera*> m_cameras;
};
