#pragma once

#include "glTFRenderInterfaceShaderResourceView.h"
#include "glTFRenderInterfaceSingleConstantBuffer.h"
#include "glm/glm/mat4x4.hpp"

struct ConstantBufferSceneMesh
{
    glm::mat4 worldMat {glm::mat4{1.0f}};
    glm::mat4 transInvWorldMat {glm::mat4{1.0f}};
    bool using_normal_mapping {false};
};

class glTFRenderInterfaceSceneMesh: public glTFRenderInterfaceSingleConstantBuffer<ConstantBufferSceneMesh>
{
public:
    virtual void UpdateShaderCompileDefine(RHIShaderPreDefineMacros& outShaderPreDefineMacros) const override;
};

class glTFRenderInterfaceSceneMeshMaterial : public glTFRenderInterfaceShaderResourceView
{
public:
    virtual void UpdateShaderCompileDefine(RHIShaderPreDefineMacros& outShaderPreDefineMacros) const override;
};