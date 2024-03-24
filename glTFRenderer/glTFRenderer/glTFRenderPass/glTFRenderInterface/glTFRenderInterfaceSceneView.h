#pragma once

#include "glm/glm/mat4x4.hpp"
#include "glTFRenderInterfaceSingleConstantBuffer.h"

class glTFSceneView;

ALIGN_FOR_CBV_STRUCT struct ConstantBufferSceneView
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

class glTFRenderInterfaceSceneView : public glTFRenderInterfaceSingleConstantBuffer<ConstantBufferSceneView>
{
public:
    glTFRenderInterfaceSceneView();
    
    bool UpdateSceneView(const glTFSceneView& view);

protected:
    ConstantBufferSceneView m_scene_view;
};