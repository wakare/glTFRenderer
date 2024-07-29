#ifndef SCENE_MESH
#define SCENE_MESH

#include "glTFResources/ShaderSource/ShaderDeclarationUtil.hlsl"

struct SceneMeshConstantBuffer 
{
    uint instance_offset;
};
DECLARE_RESOURCE(ConstantBuffer<SceneMeshConstantBuffer> instance_offset_buffer, SCENE_MESH_REGISTER_CBV_INDEX);
#endif