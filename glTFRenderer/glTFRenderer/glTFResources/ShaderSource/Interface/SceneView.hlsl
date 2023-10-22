#ifndef SCENE_VIEW
#define SCENE_VIEW

cbuffer SceneViewConstantBuffer : SCENE_VIEW_REGISTER_INDEX
{
    // Current matrix
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4x4 inverseViewMatrix;
    float4x4 inverseProjectionMatrix;

    // Prev matrix
    float4x4 prev_view_matrix;
    float4x4 prev_projection_matrix;
    
    float4 view_position;
    uint viewport_width;
    uint viewport_height;
};
#endif