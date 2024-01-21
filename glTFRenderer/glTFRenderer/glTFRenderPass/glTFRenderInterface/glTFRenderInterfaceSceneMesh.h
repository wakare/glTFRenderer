#pragma once

#include "glTFRenderInterfaceSingleConstantBuffer.h"

ALIGN_FOR_CBV_STRUCT struct ConstantBufferSceneMesh
{
    inline static std::string Name = "SCENE_MESH_REGISTER_CBV_INDEX";
    
    unsigned instance_offset;
};

typedef glTFRenderInterfaceSingleConstantBuffer<ConstantBufferSceneMesh> glTFRenderInterfaceSceneMesh;