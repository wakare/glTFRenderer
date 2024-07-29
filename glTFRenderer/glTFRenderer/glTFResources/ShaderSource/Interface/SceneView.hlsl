#ifndef SCENE_VIEW
#define SCENE_VIEW

#include "glTFResources/ShaderSource/ShaderDeclarationUtil.hlsl"

DECLARE_RESOURCE(cbuffer SceneViewConstantBuffer , SCENE_VIEW_REGISTER_INDEX)
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
    
    float nearZ;
    float farZ;

    float4 left_plane_normal;
    float4 right_plane_normal;
    float4 up_plane_normal;
    float4 down_plane_normal;
};
#endif