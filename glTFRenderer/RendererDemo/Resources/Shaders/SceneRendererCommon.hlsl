#ifndef SCENE_RENDERER_COMMON_HLSL
#define SCENE_RENDERER_COMMON_HLSL

#include "RendererModule/RendererModuleMaterial.hlsl"

#ifdef VERTEX_SHADER
#include "SceneViewCommon.hlsl"
#endif

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

float3 GetWorldNormal(float3x3 world_matrix, float3 geometry_normal, float4 geometry_tangent, float3 tangent_normal)
{
    float3 tmpTangent = normalize(mul(world_matrix, geometry_tangent.xyz));
    float3 bitangent = cross(geometry_normal, tmpTangent) * geometry_tangent.w;
    float3 tangent = cross(bitangent, geometry_normal);
    float3x3 TBN = transpose(float3x3(tangent, bitangent, geometry_normal));
    return normalize(mul(world_matrix, mul(TBN, tangent_normal)));
}

#endif
