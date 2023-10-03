#ifndef SCENE_MESH
#define SCENE_MESH

cbuffer SceneMeshConstantBuffer : SCENE_MESH_REGISTER_CBV_INDEX
{
    float4x4 worldMat;
    float4x4 transInvWorldMat;
    bool using_normal_mapping;
};

Texture2D baseColor_texture: SCENE_MESH_REGISTER_SRV_INDEX_ZERO;
Texture2D normal_texture: SCENE_MESH_REGISTER_SRV_INDEX_ONE;

#endif