#include "glTFResources/ShaderSource/Lighting/LightingCommon.hlsl"
#include "glTFResources/ShaderSource/LightPassCommon.hlsl"

Texture2D albedoTex: ALBEDO_TEX_REGISTER_INDEX;
Texture2D depthTex: DEPTH_TEX_REGISTER_INDEX;
Texture2D normalTex: NORMAL_TEX_REGISTER_INDEX;

SamplerState defaultSampler : DEFAULT_SAMPLER_REGISTER_INDEX;

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

float4 main(VS_OUTPUT input) : SV_TARGET
{
#ifdef HAS_TEXCOORD
    float2 uv = input.texCoord;
#else
    float2 uv = 0;
#endif
    float3 world_position = GetWorldPosition(uv);
#ifdef BYPASS
    float3 final_lighting = albedoTex.Sample(defaultSampler, uv).xyz;
#else 
    float3 final_lighting = (float3)0.0;
    float3 base_color = albedoTex.Sample(defaultSampler, uv).xyz;
    float3 normal = normalize(2 * normalTex.Sample(defaultSampler, uv).xyz - 1);
    
    final_lighting = GetLighting(world_position, base_color, normal);
#endif
    
    return float4(final_lighting, 1.0);
}