#include "glTFResources/ShaderSource/Interface/LightingInterface.hlsl"
#include "glTFResources/ShaderSource/LightPassCommon.hlsl"

Texture2D albedoTex: register(t0);
Texture2D depthTex: register(t1);
Texture2D normalTex: register(t2);

RWTexture2D<float4> Output: register(u0);

float3 GetWorldPosition(int2 texCoord)
{
    float depth = depthTex.Load(int3(texCoord, 0)).r;

    float2 uv = texCoord / float2(viewport_width, viewport_height);
    
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
    float3 worldPosition = GetWorldPosition(dispatchThreadID.xy);
    
    float3 FinalLighting = (float3)0.0;
    float3 baseColor = albedoTex.Load(int3(dispatchThreadID.xy, 0)).xyz;
    float3 normal = normalize(2 * normalTex.Load(int3(dispatchThreadID.xy, 0)).xyz - 1);
    
    for (int i = 0; i < PointLightCount; ++i)
    {
        FinalLighting += LightingWithPointLight(worldPosition, baseColor, normal, g_pointLightInfos[i]);
    }

    for (int j = 0; j < DirectionalLightCount; ++j)
    {
        FinalLighting += LightingWithDirectionalLight(worldPosition, baseColor, normal, g_directionalLightInfos[j]);
    }
    
    Output[dispatchThreadID.xy] = float4(FinalLighting, 1.0);
}