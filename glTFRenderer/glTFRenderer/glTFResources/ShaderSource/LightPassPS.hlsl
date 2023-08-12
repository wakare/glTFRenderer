#include "glTFResources/ShaderSource/LightPassCommon.hlsl"

Texture2D albedoTex: register(t0);
Texture2D depthTex: register(t1);
Texture2D normalTex: register(t2);

SamplerState defaultSampler : register(s0);

cbuffer LightInfoConstantBuffer : register(b1)
{
    int PointLightCount;
    int DirectionalLightCount;
};

struct PointLightInfo
{
    float4 positionAndRadius;
    float4 intensityAndFalloff;
};

StructuredBuffer<PointLightInfo> g_pointLightInfos : register(t3);

struct DirectionalLightInfo
{
    float4 directionalAndIntensity;
};

StructuredBuffer<DirectionalLightInfo> g_directionalLightInfos : register(t4);

float3 GetWorldPosition(float2 uv)
{
    float depth = depthTex.Sample(defaultSampler, uv).r;

    // Flip uv.y is important
    float4 clipSpaceCoord = float4(2 * uv.x - 1.0, 1 - 2 * uv.y, depth, 1.0);
    float4 viewSpaceCoord = mul(inverseProjectionMatrix, clipSpaceCoord);
    viewSpaceCoord /= viewSpaceCoord.w;

    float4 worldSpaceCoord = mul(inverseViewMatrix, viewSpaceCoord);
    return worldSpaceCoord.xyz;
}

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

float4 main(VS_OUTPUT input) : SV_TARGET
{
    float2 uv = input.texCoord;
    float3 worldPosition = GetWorldPosition(uv);
#ifdef BYPASS
    float3 FinalLighting = albedoTex.Sample(defaultSampler, uv).xyz;
#else 
    float3 FinalLighting = (float3)0.0;
    float3 baseColor = albedoTex.Sample(defaultSampler, uv).xyz;
    float3 normal = normalize(2 * normalTex.Sample(defaultSampler, uv).xyz - 1);
    
    for (int i = 0; i < PointLightCount; ++i)
    {
        FinalLighting += LightingWithPointLight(worldPosition, baseColor, normal, g_pointLightInfos[i]);
    }

    for (int j = 0; j < DirectionalLightCount; ++j)
    {
        FinalLighting += LightingWithDirectionalLight(worldPosition, baseColor, normal, g_directionalLightInfos[j]);
    }
#endif
    
    return float4(FinalLighting, 1.0);
}