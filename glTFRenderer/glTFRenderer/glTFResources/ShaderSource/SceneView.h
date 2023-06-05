#ifndef SCENE_VIEW_REGISTER_INDEX
#define SCENE_VIEW_REGISTER_INDEX register(b0)
#endif

cbuffer SceneViewConstantBuffer : SCENE_VIEW_REGISTER_INDEX
{
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4x4 inverseViewMatrix;
    float4x4 inverseProjectionMatrix;
};
