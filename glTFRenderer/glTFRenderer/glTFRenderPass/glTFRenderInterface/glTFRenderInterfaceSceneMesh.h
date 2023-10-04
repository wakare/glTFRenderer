#pragma once

#include "glTFRenderInterfaceShaderResourceView.h"
#include "glTFRenderInterfaceSingleConstantBuffer.h"

struct ConstantBufferSceneMesh
{
    inline static std::string Name = "SCENE_MESH_REGISTER_CBV_INDEX";
    
    glm::mat4 worldMat {glm::mat4{1.0f}};
    glm::mat4 transInvWorldMat {glm::mat4{1.0f}};
    bool using_normal_mapping {false};
};

class glTFRenderInterfaceSceneMesh: public glTFRenderInterfaceSingleConstantBuffer<ConstantBufferSceneMesh>
{
};

class glTFRenderInterfaceSceneMeshMaterial : public glTFRenderInterfaceShaderResourceView
{
public:
    virtual void UpdateShaderCompileDefine(RHIShaderPreDefineMacros& outShaderPreDefineMacros) const override;
};