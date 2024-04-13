#pragma once

#include "glTFSceneGraph.h"
#include "glTFScenePrimitive.h"
#include "glTFCamera.h"

class glTFRenderPassManager;
class glTFInputManager;

// Resolve specific render pass with drawable primitive and handle render scene graph with camera
class glTFSceneView
{
public:
    glTFSceneView(const glTFSceneGraph& graph);

    // Only traverse necessary primitives
    void TraverseSceneObjectWithinView(const std::function<bool(const glTFSceneNode& primitive)>& visitor) const;

    glm::mat4 GetViewProjectionMatrix() const;
    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix() const;
    
    void ApplyInput(const glTFInputManager& input_manager, size_t delta_time_ms) const;
    void GetViewportSize(unsigned& out_width, unsigned& out_height) const;
    
    glTFCamera* GetMainCamera() const;
    void Tick(const glTFSceneGraph& scene_graph);

    bool GetLightingDirty() const;
    
private:
    void FocusSceneCenter(glTFCamera& camera) const;
    static void ApplyInputForCamera(const glTFInputManager& input_manager, glTFCamera& camera, size_t delta_time_ms);

    const glTFSceneGraph& m_scene_graph;

    bool m_lighting_dirty;
};
