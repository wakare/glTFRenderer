#pragma once

#include "glTFRenderInterfaceSingleConstantBuffer.h"

struct ConstantBufferSceneMesh
{
    inline static std::string Name = "SCENE_MESH_REGISTER_CBV_INDEX";
    
    unsigned instance_offset;
};

typedef glTFRenderInterfaceSingleConstantBuffer<ConstantBufferSceneMesh, 128ull * 1024> glTFRenderInterfaceSceneMesh;