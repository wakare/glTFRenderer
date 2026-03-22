#ifndef SCENE_VIEW_COMMON
#define SCENE_VIEW_COMMON

// View data
cbuffer ViewBuffer
{
    float4x4 view_projection_matrix;
    float4x4 prev_view_projection_matrix;
    float4x4 inverse_view_projection_matrix;
    float4x4 view_matrix;
    float4x4 projection_matrix;
    float4x4 inverse_projection_matrix;
    float4 view_position;
    float4 projection_params;
    
    uint viewport_width;
    uint viewport_height;
    uint2 padding;
};

#endif
