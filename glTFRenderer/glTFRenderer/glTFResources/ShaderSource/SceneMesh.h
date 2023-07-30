#ifndef SCENE_MESH_REGISTER_CBV_INDEX
#define SCENE_MESH_REGISTER_CBV_INDEX register(b0) 
#endif

#ifndef SCENE_MESH_REGISTER_SRV_INDEX_ZERO
#define SCENE_MESH_REGISTER_SRV_INDEX_ZERO register(t0)
#endif

#ifndef SCENE_MESH_REGISTER_SRV_INDEX_ONE
#define SCENE_MESH_REGISTER_SRV_INDEX_ONE register(t1)
#endif

cbuffer SceneMeshConstantBuffer : SCENE_MESH_REGISTER_CBV_INDEX
{
    float4x4 worldMat;
    float4x4 transInvWorldMat;
    bool using_normal_mapping;
};

Texture2D baseColor_texture: SCENE_MESH_REGISTER_SRV_INDEX_ZERO;
Texture2D normal_texture: SCENE_MESH_REGISTER_SRV_INDEX_ONE;