#pragma once

#include "glm/glm/mat4x4.hpp"
#include "glTFRenderInterfaceSingleConstantBuffer.h"

struct ConstantBufferSceneView
{
    glm::mat4 viewMatrix {glm::mat4{1.0f}};
    glm::mat4 ProjectionMatrix {glm::mat4{1.0f}};
    glm::mat4 inverseViewMatrix {glm::mat4{1.0f}};
    glm::mat4 inverseProjectionMatrix {glm::mat4{1.0f}};

    unsigned viewport_width {1920};
    unsigned viewport_height {1080};
};

class glTFRenderInterfaceSceneView : public glTFRenderInterfaceSingleConstantBuffer<ConstantBufferSceneView>
{
public:
    virtual void UpdateShaderCompileDefine(RHIShaderPreDefineMacros& outShaderPreDefineMacros) const override;
};