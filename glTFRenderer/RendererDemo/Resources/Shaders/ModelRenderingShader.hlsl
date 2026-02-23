#include "Resources/Shaders/SceneRendererCommon.hlsl"

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : UV;
    uint vs_material_id: MATERIAL_ID;
    float3 normal: NORMAL;
    float4 tangent: TANGENT;
    float3x3 world_rotation_matrix: WORLD_ROTATION_MATRIX;
    float4 current_clip_pos : CURRENT_CLIP_POS;
    float4 prev_clip_pos : PREV_CLIP_POS;
};

#ifdef VERTEX_SHADER
VSOutput MainVS(uint Vertex_ID : SV_VertexID, uint Instance_ID : SV_InstanceID
#ifdef DX_SHADER
    , uint StartInstanceOffset : SV_StartInstanceLocation
#endif
    )
{
    VSOutput output;
    uint instance_id = Instance_ID
#ifdef DX_SHADER
         + StartInstanceOffset
#endif
    ;
    MeshInstanceInputData instance_input_data = mesh_instance_input_data[instance_id];
    float4x4 instance_transform = transpose(instance_input_data.instance_transform);
    uint index = mesh_start_info[instance_input_data.mesh_id].start_vertex_index + Vertex_ID;
    SceneMeshVertexInfo vertex = mesh_vertex_info[index];
    
    float4 world_pos = mul(instance_transform, float4(vertex.position.xyz, 1.0));
    output.pos = mul(view_projection_matrix, world_pos);
    output.current_clip_pos = output.pos;
    output.prev_clip_pos = mul(prev_view_projection_matrix, world_pos);
    //output.color = float3(1.0, 1.0, 1.0);
    output.vs_material_id = mesh_start_info[instance_input_data.mesh_id].material_index;
    output.uv = vertex.uv.xy;
    output.tangent = vertex.tangent;
    output.world_rotation_matrix = (float3x3)instance_transform;
    output.normal = normalize(mul(instance_transform, float4(vertex.normal.xyz, 0.0)).xyz);

    return output;
}
#endif

struct FSOutput
{
    float4 color : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float4 velocity : SV_TARGET2;
};

#ifdef PIXEL_SHADER
FSOutput MainFS(VSOutput input)
{
    FSOutput output;
    
    output.color = SampleAlbedoTexture(input.vs_material_id, input.uv.xy);
    
    float3 normal = normalize(2 * SampleNormalTexture(input.vs_material_id, input.uv).xyz - 1.0);
    output.normal = float4(GetWorldNormal(input.world_rotation_matrix, input.normal, input.tangent, normal), 0.0);

    output.normal = normalize(output.normal) * 0.5 + 0.5;

    float2 velocity_uv = float2(0.0, 0.0);
    if (abs(input.current_clip_pos.w) > 1e-6 && abs(input.prev_clip_pos.w) > 1e-6)
    {
        const float2 current_ndc = input.current_clip_pos.xy / input.current_clip_pos.w;
        const float2 prev_ndc = input.prev_clip_pos.xy / input.prev_clip_pos.w;
        velocity_uv = float2(
            (current_ndc.x - prev_ndc.x) * 0.5,
            (prev_ndc.y - current_ndc.y) * 0.5);
    }
    output.velocity = float4(velocity_uv, 0.0, 0.0);
    
    return output;
}
#endif
