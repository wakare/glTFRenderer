#ifndef INTERFACE_LIGHTING
#define INTERFACE_LIGHTING

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

float3 LightingWithPointLight(float3 worldPosition, float3 baseColor, float3 normal, PointLightInfo pointLightInfo)
{
    float3 pointLightPosition = pointLightInfo.positionAndRadius.xyz;
    float3 normalizedLightDir = normalize(pointLightPosition - worldPosition); 
    float geometryFalloff = max(0.0, dot(normal, normalizedLightDir));
    
    float pointLightRadius = pointLightInfo.positionAndRadius.w;
    float pointLightFalloff = pointLightInfo.intensityAndFalloff.y;
    float lightIntensity = 1.0 - saturate(length(worldPosition - pointLightPosition) / pointLightRadius);
    
    return baseColor * geometryFalloff * pointLightInfo.intensityAndFalloff.x * pow(lightIntensity, pointLightFalloff);
}

float3 LightingWithDirectionalLight(float3 worldPosition, float3 baseColor, float3 normal, DirectionalLightInfo directionalLightInfo)
{
    float3 normalizedLightDir = normalize(directionalLightInfo.directionalAndIntensity.xyz);
    float intensity = directionalLightInfo.directionalAndIntensity.w;
    float geometryFalloff = max(0.0, dot(normal, -normalizedLightDir));
    geometryFalloff = pow(geometryFalloff, 2.0);
    
    return baseColor * intensity * geometryFalloff;
}

#endif