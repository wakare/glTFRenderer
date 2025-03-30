#ifndef MESH_PASS_COMMON
#define MESH_PASS_COMMON
#include "glTFResources/ShaderSource/Interface/SceneView.hlsl"
#include "glTFResources/ShaderSource/ShaderDeclarationUtil.hlsl"

struct VS_INPUT
{
    DECLARE_ATTRIBUTE_LOCATION(float3 pos, POSITION);
    
#ifdef HAS_NORMAL 
    DECLARE_ATTRIBUTE_LOCATION(float3 normal, NORMAL);
#endif
    
#ifdef HAS_TANGENT
    DECLARE_ATTRIBUTE_LOCATION(float4 tangent, TANGENT);
#endif
    
#ifdef HAS_TEXCOORD 
    DECLARE_ATTRIBUTE_LOCATION(float2 texCoord, TEXCOORD);
#endif

    DECLARE_ATTRIBUTE_LOCATION(float4 instance_matrix_0, INSTANCE_TRANSFORM_MATRIX0);
    DECLARE_ATTRIBUTE_LOCATION(float4 instance_matrix_1, INSTANCE_TRANSFORM_MATRIX1);
    DECLARE_ATTRIBUTE_LOCATION(float4 instance_matrix_2, INSTANCE_TRANSFORM_MATRIX2);
    DECLARE_ATTRIBUTE_LOCATION(float4 instance_matrix_3, INSTANCE_TRANSFORM_MATRIX3);
    
    DECLARE_ATTRIBUTE_LOCATION(uint4 instance_custom_data, INSTANCE_CUSTOM_DATA);

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
DECLARE_RESOURCE(StructuredBuffer<MeshInstanceInputData> g_mesh_instance_input_data, MESH_INSTANCE_INPUT_DATA_REGISTER_SRV_INDEX);

// This function estimates mipmap levels
float MipLevel(float2 uv, float size)
{
    float2 dx = ddx(uv * size);
    float2 dy = ddy(uv * size);
    float d = max(dot(dx, dx), dot(dy, dy));

    return max(0.5 * log2(d), 0);
}

#endif