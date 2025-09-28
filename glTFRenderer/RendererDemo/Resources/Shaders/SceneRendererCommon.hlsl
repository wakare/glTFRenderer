#ifndef SCENE_RENDERER_COMMON_HLSL
#define SCENE_RENDERER_COMMON_HLSL

// Mesh data
struct SceneMeshVertexInfo
{
    float4 position;
    float4 normal;
    float4 tangent;
    float4 uv;
};
StructuredBuffer<SceneMeshVertexInfo> mesh_vertex_info;

struct SceneMeshDataOffsetInfo
{
    uint start_face_index; 
    uint material_index;
    uint start_vertex_index; // -- vertex info start index
};
StructuredBuffer<SceneMeshDataOffsetInfo> mesh_start_info;

struct MeshInstanceInputData
{
    float4x4 instance_transform;
    uint instance_material_id;
    uint mesh_id; // -- mesh start info index
    uint2 padding;
};
StructuredBuffer<MeshInstanceInputData> mesh_instance_input_data;

// View data
cbuffer ViewBuffer
{
    float4x4 view_projection_matrix;
};

#endif