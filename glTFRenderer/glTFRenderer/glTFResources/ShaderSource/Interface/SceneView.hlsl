#ifndef SCENE_VIEW
#define SCENE_VIEW

cbuffer SceneViewConstantBuffer : SCENE_VIEW_REGISTER_INDEX
{
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4x4 inverseViewMatrix;
    float4x4 inverseProjectionMatrix;
    float4 view_position;
    uint viewport_width;
    uint viewport_height;
};
#endif