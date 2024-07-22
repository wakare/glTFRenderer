#ifndef SCENE_MESH
#define SCENE_MESH

struct SceneMeshConstantBuffer 
{
    uint instance_offset;
};
ConstantBuffer<SceneMeshConstantBuffer> instance_offset_buffer : SCENE_MESH_REGISTER_CBV_INDEX;
#endif