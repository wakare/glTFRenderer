#ifndef MESH_PASS_COMMON
#define MESH_PASS_COMMON
#include "glTFResources/ShaderSource/Interface/SceneView.hlsl"
//#include "glTFResources/ShaderSource/Interface/SceneMesh.hlsl"

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
    
    uint4 instance_custom_data : INSTANCE_CUSTOM_DATA;

    uint GetMaterialID() {return instance_custom_data.x; }
    uint NormalMapping() {return instance_custom_data.y;}
    uint GetMeshID() {return instance_custom_data.z;}
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
    uint normal_mapping: NORMAL_MAPPING;
    float3x3 world_rotation_matrix: WORLD_ROTATION_MATRIX;
};

#define PS_INPUT VS_OUTPUT

struct PS_OUTPUT
{
    float4 baseColor: SV_TARGET0;
    float4 normal: SV_TARGET1;
};

struct MeshInstanceInputData
{
    float4x4 instance_transform;
    uint instance_material_id;
    uint normal_mapping;
    uint mesh_id;
    uint padding;
};
StructuredBuffer<MeshInstanceInputData> g_mesh_instance_input_data : MESH_INSTANCE_INPUT_DATA_REGISTER_SRV_INDEX;

#endif