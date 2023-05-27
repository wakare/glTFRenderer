#include "glTFResources/ShaderSource/LightPassCommon.h"

Texture2D albedoTex: register(t0);
Texture2D depthTex: register(t1);
SamplerState defaultSampler : register(s0);

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
    float lightIntensity = length(worldPosition - float4(0.0, 0.0, 0.0, 0.0)) < 5 ? 1.0 : 0.0; 
    
    return albedoTex.Sample(defaultSampler, input.texCoord) * lightIntensity;
}