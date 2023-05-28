#include "glTFResources/ShaderSource/LightPassCommon.h"

Texture2D albedoTex: register(t0);
Texture2D depthTex: register(t1);
SamplerState defaultSampler : register(s0);

cbuffer ConstantBuffer : register(b2)
{
    float4 PointLightInfos[4];
};

float4 GetWorldPosition(float2 uv)
{
    float depth = depthTex.Sample(defaultSampler, uv).r;
    float4 viewportCoord = float4(uv * 2 - 1.0, depth, 1.0);
    float4 worldPosition = mul(inverseViewProjectionMat, viewportCoord);
    return worldPosition / worldPosition.w;
}

float4 main(VS_OUTPUT input) : SV_TARGET
{
    float4 worldPosition = GetWorldPosition(input.texCoord);
    float4 FinalLighting = (float4)0.0;
    float4 baseColor = albedoTex.Sample(defaultSampler, input.texCoord);
    
    for (int i = 0; i < 4; ++i)
    {
        float4 PointLightPosition = float4(PointLightInfos[i].xyz, 1.0);
        float PointLightRadius = PointLightInfos[i].w;
        float lightIntensity = 1.0 - saturate(length(worldPosition - PointLightPosition) / PointLightRadius);  
        FinalLighting += baseColor * pow(lightIntensity, 2.0);
    }
    
    return FinalLighting;
}