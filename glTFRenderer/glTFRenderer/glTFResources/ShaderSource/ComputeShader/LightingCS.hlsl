#include "glTFResources/ShaderSource/Lighting/LightingCommon.hlsl"
#include "glTFResources/ShaderSource/LightPassCommon.hlsl"
#include "glTFResources/ShaderSource/Math/MathCommon.hlsl"

Texture2D albedoTex: ALBEDO_TEX_REGISTER_INDEX;
Texture2D normalTex: NORMAL_TEX_REGISTER_INDEX;

Texture2D depthTex: DEPTH_TEX_REGISTER_INDEX;

RWTexture2D<float4> Output: OUTPUT_TEX_REGISTER_INDEX;
SamplerState defaultSampler : DEFAULT_SAMPLER_REGISTER_INDEX;

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

    float4 albedo_buffer_data = albedoTex.Load(int3(dispatchThreadID.xy, 0));
    float4 normal_buffer_data = normalTex.Load(int3(dispatchThreadID.xy, 0));
    
    float3 base_color = albedo_buffer_data.xyz;
    float metallic = albedo_buffer_data.w;
    
    float3 normal = normalize(2 * normal_buffer_data.xyz - 1);
    float roughness = normal_buffer_data.w;

    float3 view = normalize(view_position.xyz - world_position);
    
    float3 final_lighting = GetLighting(world_position, base_color, normal, metallic, roughness, view);
    
    Output[dispatchThreadID.xy] = float4(LinearToSrgb(final_lighting), 1.0);
}