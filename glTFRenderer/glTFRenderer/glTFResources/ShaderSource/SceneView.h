#ifndef SCENE_VIEW_REGISTER_INDEX
#define SCENE_VIEW_REGISTER_INDEX register(b0)
#endif

cbuffer ConstantBuffer : SCENE_VIEW_REGISTER_INDEX
{
    float4x4 viewProjectionMat;
    float4x4 inverseViewProjectionMat;
};
