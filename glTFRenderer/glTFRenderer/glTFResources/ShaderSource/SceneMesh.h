#ifndef SCENE_MESH_REGISTER_INDEX
#define SCENE_MESH_REGISTER_INDEX register(b0) 
#endif

cbuffer ConstantBuffer : SCENE_MESH_REGISTER_INDEX
{
    float4x4 worldMat;
};