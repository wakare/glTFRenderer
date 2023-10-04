#pragma once

#include "glm/glm/mat4x4.hpp"
#include "glTFRenderInterfaceSingleConstantBuffer.h"

struct ConstantBufferSceneView
{
    inline static std::string Name = "SCENE_VIEW_REGISTER_INDEX"; 
    glm::mat4 viewMatrix {glm::mat4{1.0f}};
    glm::mat4 ProjectionMatrix {glm::mat4{1.0f}};
    glm::mat4 inverseViewMatrix {glm::mat4{1.0f}};
    glm::mat4 inverseProjectionMatrix {glm::mat4{1.0f}};
    // TODO: Be careful for adding member to ConstantBuffer with alignment!
    glm::float4 view_position {0.0f};
    unsigned viewport_width {0};
    unsigned viewport_height {0};
};

class glTFRenderInterfaceSceneView : public glTFRenderInterfaceSingleConstantBuffer<ConstantBufferSceneView>
{
};