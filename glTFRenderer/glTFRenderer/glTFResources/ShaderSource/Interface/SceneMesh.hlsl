#ifndef SCENE_MESH
#define SCENE_MESH

cbuffer SceneMeshConstantBuffer : SCENE_MESH_REGISTER_CBV_INDEX
{
    uint instance_offset;
};

#endif