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
    glTFRenderInterfaceSceneMesh(unsigned root_parameter_cbv_index, unsigned register_index);

    virtual void UpdateShaderCompileDefine(RHIShaderPreDefineMacros& outShaderPreDefineMacros) const override;
};

class glTFRenderInterfaceSceneMeshMaterial : public glTFRenderInterfaceShaderResourceView
{
public:
    glTFRenderInterfaceSceneMeshMaterial(
        unsigned root_parameter_srv_index,
        unsigned srv_register_index,
        unsigned max_srv_count);

    virtual void UpdateShaderCompileDefine(RHIShaderPreDefineMacros& outShaderPreDefineMacros) const override;
};