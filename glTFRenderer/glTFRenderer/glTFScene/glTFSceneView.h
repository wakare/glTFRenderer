#pragma once

#include "glTFSceneGraph.h"
#include "glTFScenePrimitive.h"
#include "glTFCamera.h"

class glTFRenderPassManager;
class glTFInputManager;

// Flags representation: https://dietertack.medium.com/using-bit-flags-in-c-d39ec6e30f08
struct glTFSceneViewRenderFlags
{
    enum Flags
    {
        Lit = 1 << 0,
    };

    void SetFlag(Flags flag) { m_flags |= flag; }
    void UnsetFlag(Flags flag) { m_flags &= ~static_cast<int>(flag); }
    bool IsFlagSet(Flags flag) const { return m_flags & flag; }
    
    void SetEnableLit(bool enable)
    {
        if (enable)
        {
            SetFlag(Lit);
        }
        else
        {
            UnsetFlag(Lit);
        }
    }
    
    bool IsLit() const {return IsFlagSet(Lit); }

    uint64_t m_flags { 0llu };
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
    
private:
    glTFCamera* GetMainCamera() const;
    void FocusSceneCenter(glTFCamera& camera) const;
    void ApplyInputForCamera(glTFInputManager& input_manager, glTFCamera& camera, size_t delta_time_ms) const;

    glTFSceneViewRenderFlags m_render_flags;
    
    const glTFSceneGraph& m_scene_graph;
    std::vector<glTFCamera*> m_cameras;
};
