#ifndef INDIRECT_CULLING
#define INDIRECT_CULLING
#include "glTFResources/ShaderSource/Interface/SceneView.hlsl"

#ifndef threadBlockSize
#define threadBlockSize 64
#endif

//#define NDC_CULLING

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
    uint start_index_location;
    uint vertex_location;
    uint start_instance_location;
    
    uint padding;
};
StructuredBuffer<IndirectDrawCommand> g_indirect_draw_data : INDIRECT_DRAW_DATA_REGISTER_SRV_INDEX;

struct CullingBoundingBox
{
    float4 bounding_box;
};
StructuredBuffer<CullingBoundingBox> g_bounding_box_data : CULLING_BOUNDING_BOX_REGISTER_SRV_INDEX;

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
        CullingBoundingBox box = g_bounding_box_data[instance_input_data.mesh_id];

        /*
        float4 instance_transform_vec0 = float4(instance_input_data.instance_transform[0].xyz, 0.0);
        float4 instance_transform_vec1 = float4(instance_input_data.instance_transform[1].xyz, 0.0);
        float4 instance_transform_vec2 = float4(instance_input_data.instance_transform[2].xyz, 0.0);
        float instance_max_radius = max(length(instance_transform_vec0), max(length(instance_transform_vec1), length(instance_transform_vec2)));
        */        
        float4 instance_box_radius = mul(instance_transform, float4(box.bounding_box.w, box.bounding_box.w, box.bounding_box.w, 0.0));
        float instance_max_radius = max(instance_box_radius.x, max(instance_box_radius.y, instance_box_radius.z));

        float4 box_center_world_space = mul(instance_transform, float4(box.bounding_box.xyz, 1.0));
        float4 box_center_view_space = mul(viewMatrix, box_center_world_space); 
        box_center_view_space /= box_center_view_space.w;

#ifdef NDC_CULLING
        float4 box_center_ndc = mul(projectionMatrix, box_center_view_space);
        box_center_ndc /= box_center_ndc.w;
        bool instance_culled =
            box_center_ndc.x < -1.0 || box_center_ndc.x > 1.0 ||
            box_center_ndc.y < -1.0 || box_center_ndc.y > 1.0 ||
            box_center_ndc.z < 0.0 || box_center_ndc.z > 1.0;
#else
        float distance_left_plane = dot(box_center_view_space, left_plane_normal);
        float distance_right_plane = dot(box_center_view_space, right_plane_normal);
        float distance_up_plane = dot(box_center_view_space, up_plane_normal);
        float distance_down_plane = dot(box_center_view_space, down_plane_normal);

        bool instance_culled =        
            box_center_view_space.z < (nearZ - instance_max_radius) ||
            box_center_view_space.z > (farZ + instance_max_radius) ||
            distance_left_plane < -instance_max_radius ||
            distance_right_plane < -instance_max_radius ||
            distance_up_plane < -instance_max_radius ||
            distance_down_plane < -instance_max_radius
            ;
#endif
        culled &= instance_culled;
    }

    if (!culled)
    {
        culled_indirect_commands.Append(g_indirect_draw_data[index]);    
    }
}

#endif