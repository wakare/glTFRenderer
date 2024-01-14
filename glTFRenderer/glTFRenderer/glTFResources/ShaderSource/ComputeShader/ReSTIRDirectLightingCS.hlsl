#ifndef RESTIR_DIRECT_LIGHTING_CS
#define RESTIR_DIRECT_LIGHTING_CS

#include "glTFResources/ShaderSource/Lighting/LightingCommon.hlsl"
#include "glTFResources/ShaderSource/LightPassCommon.hlsl"
#include "glTFResources/ShaderSource/Math/MathCommon.hlsl"

Texture2D albedoTex: ALBEDO_REGISTER_INDEX;
Texture2D normalTex: NORMAL_REGISTER_INDEX;
Texture2D depthTex: DEPTH_REGISTER_INDEX;

Texture2D LightingSamples : LIGHTING_SAMPLES_REGISTER_INDEX;
Texture2D ScreenUVOffset : SCREEN_UV_OFFSET_REGISTER_INDEX;

RWTexture2D<float4> Output: OUTPUT_TEX_REGISTER_INDEX;

RWTexture2D<float4> aggregate_samples_output : AGGREGATE_OUTPUT_REGISTER_INDEX;
Texture2D<float4> aggregate_samples_back_buffer : AGGREGATE_BACKBUFFER_REGISTER_INDEX;

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
    Output[dispatchThreadID.xy] = 0.0;
    
    float3 world_position = GetWorldPosition(dispatchThreadID.xy);

    float4 albedo_buffer_data = albedoTex.Load(int3(dispatchThreadID.xy, 0));
    float4 normal_buffer_data = normalTex.Load(int3(dispatchThreadID.xy, 0));

    float3 albedo = albedo_buffer_data.xyz;
    float metallic = albedo_buffer_data.w;
    
    float3 normal = normalize(2 * normal_buffer_data.xyz - 1);
    float roughness = normal_buffer_data.w;

    PointLightShadingInfo shading_info;
    shading_info.albedo = albedo;
    shading_info.position = world_position;
    shading_info.normal = normal;
    shading_info.metallic = metallic;
    shading_info.roughness = roughness;
    
    float3 view = normalize(view_position.xyz - world_position);

    float4 lighting_sample = LightingSamples.Load(int3(dispatchThreadID.xy, 0));
    int light_index = round(ScreenUVOffset.Load(int3(dispatchThreadID.xy, 0)).z);
    float lighting_weight = lighting_sample.w;
    if (lighting_weight >= 0.0 && light_index >= 0)
    {
        float3 final_lighting = lighting_weight * GetLightingByIndex(light_index, shading_info, view);
        Output[dispatchThreadID.xy] = float4(LinearToSrgb(final_lighting), 1.0);    
    }
}


#endif