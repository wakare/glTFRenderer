#pragma once

#include "glTFSceneGraph.h"
#include "glTFScenePrimitive.h"
#include "glTFCamera.h"

class glTFRenderPassManager;
class glTFInputManager;

struct ConstantBufferSceneView
{
    inline static std::string Name = "SCENE_VIEW_REGISTER_INDEX"; 
    glm::mat4 view_matrix {glm::mat4{1.0f}};
    glm::mat4 projection_matrix {glm::mat4{1.0f}};
    glm::mat4 inverse_view_matrix {glm::mat4{1.0f}};
    glm::mat4 inverse_projection_matrix {glm::mat4{1.0f}};
    
    // Prev matrix
    glm::mat4 prev_view_matrix {glm::mat4{1.0f}};
    glm::mat4 prev_projection_matrix {glm::mat4{1.0f}};
    
    // TODO: Be careful for adding member to ConstantBuffer with alignment!
    glm::vec4 view_position {0.0f};
    unsigned viewport_width {0};
    unsigned viewport_height {0};

    float nearZ {0.0f};
    float farZ {0.0f};

    glm::vec4 view_left_plane_normal;
    glm::vec4 view_right_plane_normal;
    glm::vec4 view_up_plane_normal;
    glm::vec4 view_down_plane_normal;
};

// Resolve specific render pass with drawable primitive and handle render scene graph with camera
class glTFSceneView
{
public:
    glTFSceneView(const glTFSceneGraph& graph);

    std::vector<const glTFSceneNode*> GetDirtySceneNodes() const;
    
    glm::mat4 GetViewProjectionMatrix() const;
    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix() const;
    
    void ApplyInput(const glTFInputManager& input_manager, size_t delta_time_ms) const;
    void GetViewportSize(unsigned& out_width, unsigned& out_height) const;
    
    glTFCamera* GetMainCamera() const;
    void Tick(const glTFSceneGraph& scene_graph);
    void GatherDirtySceneNodes();
    
    bool GetLightingDirty() const;

    ConstantBufferSceneView CreateSceneViewConstantBuffer(glTFRenderResourceManager& resource_manager) const;   
    
private:
    // Only traverse necessary primitives
    void TraverseSceneObjectWithinView(const std::function<bool(const glTFSceneNode& primitive)>& visitor) const;
    
    void FocusSceneCenter(glTFCamera& camera) const;
    static void ApplyInputForCamera(const glTFInputManager& input_manager, glTFCamera& camera, size_t delta_time_ms);

    const glTFSceneGraph& m_scene_graph;

    bool m_lighting_dirty;
    std::vector<const glTFSceneNode*> m_frame_dirty_nodes;
};