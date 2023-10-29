#ifndef MESH_PASS_COMMON
#define MESH_PASS_COMMON
#include "glTFResources/ShaderSource/Interface/SceneView.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneMesh.hlsl"

struct VS_INPUT
{
    float3 pos: POSITION;
    
#ifdef HAS_NORMAL 
    float3 normal: NORMAL;
#endif
    
#ifdef HAS_TANGENT
    float4 tangent: TANGENT;
#endif
    
#ifdef HAS_TEXCOORD 
    float2 texCoord: TEXCOORD;
#endif

    float4 instance_matrix_0: INSTANCE_TRANSFORM_MATRIX0;
    float4 instance_matrix_1: INSTANCE_TRANSFORM_MATRIX1;
    float4 instance_matrix_2: INSTANCE_TRANSFORM_MATRIX2;
    float4 instance_matrix_3: INSTANCE_TRANSFORM_MATRIX3;
    uint instance_material_id : INSTANCE_MATERIAL_ID;
};

struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    
#ifdef HAS_NORMAL
    float3 normal: NORMAL;
#endif

#ifdef HAS_TANGENT
    float4 tangent: TANGENT;
#endif
    
#ifdef HAS_TEXCOORD
    float2 texCoord: TEXCOORD;
#endif

    uint vs_material_id: MATERIAL_ID;
};

#define PS_INPUT VS_OUTPUT

struct PS_OUTPUT
{
    float4 baseColor: SV_TARGET0;
    float4 normal: SV_TARGET1;
};
#endif