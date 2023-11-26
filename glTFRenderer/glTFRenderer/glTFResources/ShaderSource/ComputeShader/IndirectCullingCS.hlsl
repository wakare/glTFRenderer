#ifndef INDIRECT_CULLING
#define INDIRECT_CULLING
#include "glTFResources/ShaderSource/Interface/SceneView.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneMeshInfo.hlsl"

#ifndef threadBlockSize
#define threadBlockSize 64
#endif

cbuffer CullingConstant: CULLING_CONSTANT_BUFFER_REGISTER_CBV_INDEX
{
    uint input_indirect_commands_count;
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

struct IndirectDrawCommand
{
    uint4 vertex_buffer;
    uint4 instance_buffer;
    uint4 index_buffer;
    
    uint index_count_per_instance;
    uint instance_count;
    uint start_instance_location;
    uint vertex_location;
    uint start_index_location;
    uint pad;
};
StructuredBuffer<IndirectDrawCommand> g_indirect_draw_data : INDIRECT_DRAW_DATA_REGISTER_SRV_INDEX;

AppendStructuredBuffer<IndirectDrawCommand> culled_indirect_commands : INDIRECT_DRAW_DATA_OUTPUT_REGISTER_UAV_INDEX;    // UAV: Processed indirect commands

[numthreads(threadBlockSize, 1, 1)]
void main(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    uint index = (groupId.x * threadBlockSize) + groupIndex;
    if (index >= input_indirect_commands_count)
    {
        return;
    }
    
    uint instance_count = g_indirect_draw_data[index].instance_count;
    bool culled = true;
    
    for (uint instance_index = 0; instance_index < instance_count; ++instance_index)
    {
        uint instance_id = instance_index + g_indirect_draw_data[index].start_instance_location;
        MeshInstanceInputData instance_input_data = g_mesh_instance_input_data[instance_id];
        float4x4 instance_transform = transpose(instance_input_data.instance_transform);

        for (uint vertex_index = 0; vertex_index < g_indirect_draw_data[index].index_count_per_instance; ++vertex_index)
        {
            // Cull per vertex
            uint vertex_info_index = g_mesh_index_info[g_mesh_start_info[instance_input_data.mesh_id].start_index].vertex_index + vertex_index;
            SceneMeshVertexInfo vertex = g_mesh_vertex_info[vertex_info_index];

            float4x4 mvp_matrix = mul(projectionMatrix, mul(viewMatrix, instance_transform));
            float4 vertex_ndc_coord = mul(mvp_matrix, float4(vertex.position.xyz, 1.0));
            vertex_ndc_coord /= vertex_ndc_coord.w;
            
            if (abs(vertex_ndc_coord.x) < 1.0 && abs(vertex_ndc_coord.y) < 1.0)
            {
                culled = false;
                break;
            }
        }
    }

    if (!culled)
    {
        culled_indirect_commands.Append(g_indirect_draw_data[index]);    
    }
}

#endif