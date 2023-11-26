#pragma once

#include "glTFSceneGraph.h"
#include "glTFScenePrimitive.h"
#include "glTFCamera.h"

class glTFRenderPassManager;
class glTFInputManager;

enum glTFSceneViewFlags
{
    glTFSceneViewFlags_Lit = 1 << 0,
};

struct glTFSceneViewRenderFlags : public glTFFlagsBase<glTFSceneViewFlags>
{
    void SetEnableLit(bool enable)
    {
        if (enable)
        {
            SetFlag(glTFSceneViewFlags_Lit);
        }
        else
        {
            UnsetFlag(glTFSceneViewFlags_Lit);
        }
    }
    
    bool IsLit() const {return IsFlagSet(glTFSceneViewFlags_Lit); }
};

// Resolve specific render pass with drawable primitive and handle render scene graph with camera
class glTFSceneView
{
public:
    glTFSceneView(const glTFSceneGraph& graph);

	bool SetupRenderPass(glTFRenderPassManager& out_render_pass_manager) const;
    
    // Only traverse necessary primitives
    void TraverseSceneObjectWithinView(const std::function<bool(const glTFSceneNode& primitive)>& visitor);

    glm::mat4 GetViewProjectionMatrix() const;
    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix() const;
    
    
    void ApplyInput(glTFInputManager& input_manager, size_t delta_time_ms) const;
    void GetViewportSize(unsigned& out_width, unsigned& out_height) const;
    glTFCamera* GetMainCamera() const;
    
private:
    void FocusSceneCenter(glTFCamera& camera) const;
    void ApplyInputForCamera(glTFInputManager& input_manager, glTFCamera& camera, size_t delta_time_ms) const;

    glTFSceneViewRenderFlags m_render_flags;
    
    const glTFSceneGraph& m_scene_graph;
    std::vector<glTFCamera*> m_cameras;
};
