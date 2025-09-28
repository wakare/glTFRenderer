#include "Resources/Shaders/SceneRendererCommon.hlsl"

struct VSOutput
{
    float4 pos : SV_POSITION;
    float3 color : COLOR;
};

VSOutput MainVS(uint Vertex_ID : SV_VertexID, uint Instance_ID : SV_InstanceID, uint StartInstanceOffset : SV_StartInstanceLocation )
{
    VSOutput output;
    uint instance_id = Instance_ID + StartInstanceOffset;
    MeshInstanceInputData instance_input_data = mesh_instance_input_data[instance_id];
    float4x4 instance_transform = transpose(instance_input_data.instance_transform);
    uint index = mesh_start_info[instance_input_data.mesh_id].start_vertex_index + Vertex_ID;
    SceneMeshVertexInfo vertex = mesh_vertex_info[index];
    
    float4 world_pos = mul(instance_transform, float4(vertex.position.xyz, 1.0));
    output.pos = mul(view_projection_matrix, world_pos);
    output.color = float4(1.0, 1.0, 1.0, 1.0);
    
    return output;
}

struct FSOutput
{
    float4 color : SV_TARGET0;
};

FSOutput MainFS(VSOutput input)
{
    FSOutput output;
    output.color = float4(input.color, 1.0);
    
    return output;
}