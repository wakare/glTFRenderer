#pragma once

#include "glm/glm/mat4x4.hpp"
#include "glTFRenderInterfaceSingleConstantBuffer.h"

struct ConstantBufferSceneView
{
    glm::mat4 viewMatrix {glm::mat4{1.0f}};
    glm::mat4 ProjectionMatrix {glm::mat4{1.0f}};
    glm::mat4 inverseViewMatrix {glm::mat4{1.0f}};
    glm::mat4 inverseProjectionMatrix {glm::mat4{1.0f}};
};

class glTFRenderInterfaceSceneView : public glTFRenderInterfaceSingleConstantBuffer<ConstantBufferSceneView>
{
public:
    glTFRenderInterfaceSceneView(unsigned root_parameter_cbv_index, unsigned register_index);
    
    virtual void UpdateShaderCompileDefine(RHIShaderPreDefineMacros& outShaderPreDefineMacros) const override;
};