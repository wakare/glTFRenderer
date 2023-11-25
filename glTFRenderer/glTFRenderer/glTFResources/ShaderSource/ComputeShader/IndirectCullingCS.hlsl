#ifndef INDIRECT_CULLING
#define INDIRECT_CULLING
#include "glTFResources/ShaderSource/Interface/SceneView.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneMeshInfo.hlsl"

#ifndef threadBlockSize
#define threadBlockSize 64
#endif

cbuffer CullingConstant: CULLING_CONSTANT_BUFFER_REGISTER_CBV_INDEX
{
    uint command_count;
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
    
    uint4 indirect_draw_argument0;
    uint2 indirect_draw_argument1;
};
StructuredBuffer<IndirectDrawCommand> g_indirect_draw_data : INDIRECT_DRAW_DATA_REGISTER_SRV_INDEX;

AppendStructuredBuffer<IndirectDrawCommand> outputCommands : INDIRECT_DRAW_DATA_OUTPUT_REGISTER_UAV_INDEX;    // UAV: Processed indirect commands

[numthreads(threadBlockSize, 1, 1)]
void main(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    uint index = (groupId.x * threadBlockSize) + groupIndex;
    if (index < command_count)
    {
        outputCommands.Append(g_indirect_draw_data[index]);
    }
}

#endif