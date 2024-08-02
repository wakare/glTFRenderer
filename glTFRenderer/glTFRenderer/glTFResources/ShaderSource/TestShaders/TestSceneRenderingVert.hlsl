
#include "glTFResources/ShaderSource/Interface/SceneMeshInfo.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneView.hlsl"
#include "glTFResources/ShaderSource/ShaderDeclarationUtil.hlsl"

struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float3 normal: NORMAL;
    float4 tangent: TANGENT;
    float2 texCoord: TEXCOORD;

    uint vs_material_id: MATERIAL_ID;
    uint normal_mapping: NORMAL_MAPPING;
    float3x3 world_rotation_matrix: WORLD_ROTATION_MATRIX;
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

#if ENABLE_INPUT_LAYOUT

struct VS_INPUT
{
    DECLARE_ATTRIBUTE_LOCATION(float3 pos, POSITION0);
    
#ifdef HAS_NORMAL 
    DECLARE_ATTRIBUTE_LOCATION(float3 normal, NORMAL0);
#endif
    
#ifdef HAS_TANGENT
    DECLARE_ATTRIBUTE_LOCATION(float4 tangent, TANGENT0);
#endif
    
#ifdef HAS_TEXCOORD 
    DECLARE_ATTRIBUTE_LOCATION(float2 texCoord, TEXCOORD0);
#endif

    DECLARE_ATTRIBUTE_LOCATION(float4 instance_matrix_0, INSTANCE_TRANSFORM_MATRIX0);
    DECLARE_ATTRIBUTE_LOCATION(float4 instance_matrix_1, INSTANCE_TRANSFORM_MATRIX1);
    DECLARE_ATTRIBUTE_LOCATION(float4 instance_matrix_2, INSTANCE_TRANSFORM_MATRIX2);
    DECLARE_ATTRIBUTE_LOCATION(float4 instance_matrix_3, INSTANCE_TRANSFORM_MATRIX3);
    
    DECLARE_ATTRIBUTE_LOCATION(uint4 instance_custom_data, INSTANCE_CUSTOM_DATA0);

    uint GetMaterialID() {return instance_custom_data.x; }
    uint NormalMapping() {return instance_custom_data.y;}
    uint GetMeshID() {return instance_custom_data.z;}
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    float4x4 instance_transform = (float4x4(input.instance_matrix_0, input.instance_matrix_1, input.instance_matrix_2, input.instance_matrix_3));
    float4 world_pos = mul(instance_transform, float4(input.pos, 1.0));
    float4 view_pos = mul(viewMatrix, world_pos);
    output.pos = mul (projectionMatrix, view_pos);
#ifdef HAS_NORMAL
    output.normal = normalize(mul(instance_transform, float4(input.normal, 0.0)).xyz);
#endif

#ifdef HAS_TANGENT
    output.tangent = input.tangent;
#endif
    
#ifdef HAS_TEXCOORD 
    output.texCoord = input.texCoord;
#endif
    output.vs_material_id = input.GetMaterialID();
    output.normal_mapping = input.NormalMapping();
    output.world_rotation_matrix = (float3x3)instance_transform;
    
    return output;
}

#else

// SV_StartInstanceLocation is supported in sm68
#if DX_SHADER
VS_OUTPUT main(uint Vertex_ID : SV_VertexID, uint Instance_ID : SV_InstanceID, uint StartInstanceOffset : SV_StartInstanceLocation )
#else
VS_OUTPUT main(uint Vertex_ID : SV_VertexID, uint Instance_ID : SV_InstanceID )
#endif
{
    VS_OUTPUT output;

#if DX_SHADER
    uint instance_id = Instance_ID + StartInstanceOffset;
#else
    uint instance_id = Instance_ID;
#endif
    
    MeshInstanceInputData instance_input_data = g_mesh_instance_input_data[instance_id];
    float4x4 instance_transform = transpose(instance_input_data.instance_transform);
    
    uint index = g_mesh_start_info[instance_input_data.mesh_id].start_vertex_index + Vertex_ID;
    SceneMeshVertexInfo vertex = g_mesh_vertex_info[index];
    
    float4 world_pos = mul(instance_transform, float4(vertex.position.xyz, 1.0));
    float4 view_pos = mul(viewMatrix, world_pos);
    
    output.pos = mul (projectionMatrix, view_pos);
    output.normal = normalize(mul(instance_transform, float4(vertex.normal.xyz, 0.0)).xyz);
    output.tangent = vertex.tangent;
    output.texCoord = vertex.uv.xy;
    
    output.vs_material_id = instance_input_data.instance_material_id;
    output.normal_mapping = instance_input_data.normal_mapping;
    output.world_rotation_matrix = (float3x3)instance_transform;
    
    return output;
}

#endif