#include "glTFResources/ShaderSource/MeshPassCommon.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneMeshInfo.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneMesh.hlsl"

#if ENABLE_INPUT_LAYOUT
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

VS_OUTPUT main(uint Vertex_ID : SV_VertexID, uint Instance_ID : SV_InstanceID)
{
    VS_OUTPUT output;

    uint instance_id = Instance_ID + instance_offset_buffer.instance_offset;
    
    MeshInstanceInputData instance_input_data = g_mesh_instance_input_data[instance_id];
    float4x4 instance_transform = instance_input_data.instance_transform;
    
    uint index = g_mesh_start_info[instance_input_data.mesh_id].start_vertex_index + Vertex_ID;
    SceneMeshVertexInfo vertex = g_mesh_vertex_info[index];
    
    float4 world_pos = mul(instance_transform, float4(vertex.position.xyz, 1.0));
    float4 view_pos = mul(viewMatrix, world_pos);
    output.pos = mul (projectionMatrix, view_pos);
#ifdef HAS_NORMAL
    output.normal = normalize(mul(instance_transform, float4(vertex.normal.xyz, 0.0)).xyz);
#endif

#ifdef HAS_TANGENT
    output.tangent = vertex.tangent;
#endif
    
#ifdef HAS_TEXCOORD 
    output.texCoord = vertex.uv.xy;
#endif
    
    output.vs_material_id = instance_input_data.instance_material_id;
    output.normal_mapping = instance_input_data.normal_mapping;
    output.world_rotation_matrix = (float3x3)instance_transform;
    
    return output;
}

#endif