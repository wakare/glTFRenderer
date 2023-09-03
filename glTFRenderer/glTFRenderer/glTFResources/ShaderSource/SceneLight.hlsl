#ifndef SCENE_LGIHT
#define SCENE_LIGHT

cbuffer LightInfoConstantBuffer : SCENE_LIGHT_LIGHT_INFO_REGISTER_INDEX
{
    int PointLightCount;
    int DirectionalLightCount;
};

struct PointLightInfo
{
    float4 positionAndRadius;
    float4 intensityAndFalloff;
};

StructuredBuffer<PointLightInfo> g_pointLightInfos : SCENE_LIGHT_POINT_LIGHT_REGISTER_INDEX;

struct DirectionalLightInfo
{
    float4 directionalAndIntensity;
};

StructuredBuffer<DirectionalLightInfo> g_directionalLightInfos : SCENE_LIGHT_DIRECTIONAL_LIGHT_REGISTER_INDEX;

#endif