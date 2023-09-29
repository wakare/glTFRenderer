#include "glTFResources/ShaderSource/Interface/LightingInterface.hlsl"
#include "glTFResources/ShaderSource/LightPassCommon.hlsl"

Texture2D albedoTex: register(t0);
Texture2D depthTex: register(t1);
Texture2D normalTex: register(t2);

RWTexture2D<float4> Output: register(u0);
SamplerState defaultSampler : register(s0);

float3 GetWorldPosition(int2 texCoord)
{
    float depth = depthTex.Load(int3(texCoord, 0)).r;

    float2 uv = texCoord / float2(viewport_width - 1, viewport_height - 1);
    
    // Flip uv.y is important
    float4 clipSpaceCoord = float4(2 * uv.x - 1.0, 1 - 2 * uv.y, depth, 1.0);
    float4 viewSpaceCoord = mul(inverseProjectionMatrix, clipSpaceCoord);
    viewSpaceCoord /= viewSpaceCoord.w;

    float4 worldSpaceCoord = mul(inverseViewMatrix, viewSpaceCoord);
    return worldSpaceCoord.xyz;
}

[numthreads(8, 8, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    float3 world_position = GetWorldPosition(dispatchThreadID.xy);
    float depth = depthTex.Load(int3(dispatchThreadID.xy, 0)).r;
    float3 final_lighting = 0.0;
    float3 base_color = albedoTex.Load(int3(dispatchThreadID.xy, 0)).xyz;
    float3 normal = normalize(2 * normalTex.Load(int3(dispatchThreadID.xy, 0)).xyz - 1);
    
    for (int i = 0; i < PointLightCount; ++i)
    {
        final_lighting += LightingWithPointLight(world_position, base_color, normal, g_pointLightInfos[i]);
    }

    for (int j = 0; j < DirectionalLightCount; ++j)
    {
        final_lighting += LightingWithDirectionalLight(world_position, base_color, normal, g_directionalLightInfos[j]);
    }
    
    Output[dispatchThreadID.xy] = float4(final_lighting, 1.0);
}