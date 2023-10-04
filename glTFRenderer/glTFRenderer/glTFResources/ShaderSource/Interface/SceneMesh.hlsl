#ifndef SCENE_MESH
#define SCENE_MESH

cbuffer SceneMeshConstantBuffer : SCENE_MESH_REGISTER_CBV_INDEX
{
    float4x4 world_matrix;
    float4x4 inverse_world_matrix;
    uint material_id;
    bool using_normal_mapping;
};

Texture2D baseColor_texture: SCENE_MESH_REGISTER_SRV_INDEX_ZERO;
Texture2D normal_texture: SCENE_MESH_REGISTER_SRV_INDEX_ONE;

#endif