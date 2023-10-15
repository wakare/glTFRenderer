#pragma once

#include "glTFRenderInterfaceSingleConstantBuffer.h"

struct ConstantBufferSceneMesh
{
    inline static std::string Name = "SCENE_MESH_REGISTER_CBV_INDEX";
    
    glm::mat4 world_matrix {glm::mat4{1.0f}};
    glm::mat4 inverse_world_matrix {glm::mat4{1.0f}};
    unsigned material_id {UINT_MAX};
    bool using_normal_mapping {false};
};

typedef glTFRenderInterfaceSingleConstantBuffer<ConstantBufferSceneMesh> glTFRenderInterfaceSceneMesh;