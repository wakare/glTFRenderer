#ifndef RESTIR_DIRECT_LIGHTING_CS
#define RESTIR_DIRECT_LIGHTING_CS

#include "glTFResources/ShaderSource/Lighting/LightingCommon.hlsl"
#include "glTFResources/ShaderSource/LightPassCommon.hlsl"
#include "glTFResources/ShaderSource/Math/MathCommon.hlsl"
#include "glTFResources/ShaderSource/FrameStat.hlsl"

Texture2D albedoTex: ALBEDO_REGISTER_INDEX;
Texture2D normalTex: NORMAL_REGISTER_INDEX;
Texture2D depthTex: DEPTH_REGISTER_INDEX;

Texture2D LightingSamples : LIGHTING_SAMPLES_REGISTER_INDEX;
Texture2D ScreenUVOffset : SCREEN_UV_OFFSET_REGISTER_INDEX;

RWTexture2D<float4> Output: OUTPUT_TEX_REGISTER_INDEX;

RWTexture2D<float4> aggregate_samples_output : AGGREGATE_OUTPUT_REGISTER_INDEX;
Texture2D<float4> aggregate_samples_back_buffer : AGGREGATE_BACKBUFFER_REGISTER_INDEX;

SamplerState defaultSampler : DEFAULT_SAMPLER_REGISTER_INDEX;

cbuffer RayTracingDIPassOptions: RAY_TRACING_DI_POSTPROCESS_OPTION_CBV_INDEX
{
    int spatial_reuse_range;
    bool enable_spatial_reuse;
    bool enable_temporal_reuse;
};

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

void ReservoirTemporalReuse(inout RngStateType rng_state, uint2 coord, PointLightShadingInfo shading_info, float3 view, inout float4 out_sample)
{
    out_sample = 0.0;
    
    float4 lighting_sample = LightingSamples.Load(int3(coord, 0));
    int light_index = round(lighting_sample.x);
    float light_weight = lighting_sample.y;
    
    float2 prev_uv = ScreenUVOffset.Load(int3(coord, 0)).xy;
    uint2 prev_frame_coord = uint2(round(prev_uv.x * viewport_width), round(prev_uv.y * viewport_height));
    float4 old_lighting_sample = aggregate_samples_back_buffer.Load(int3(prev_frame_coord, 0));
    int old_light_index = round(old_lighting_sample.x);
    // temporal ratio to reduce old weight ()
    float old_light_weight = old_lighting_sample.y;

    if (light_index < 0 || old_light_index < 0)
    {
        out_sample.xy = old_light_weight < light_weight ? float2(light_index, light_weight): float2(old_light_index, old_light_weight);
        return;
    }

    if (light_index == old_light_index)
    {
        out_sample.xy = lighting_sample.xy;
        return;
    }
    
    Reservoir reservoir; InitReservoir(reservoir);
    AddReservoirSample(rng_state, reservoir, light_index, 0.5, luminance(GetLightingByIndex(light_index, shading_info, view)), light_weight);
    AddReservoirSample(rng_state, reservoir, old_light_index, 0.5, luminance(GetLightingByIndex(old_light_index, shading_info, view)), old_light_weight);
    GetReservoirSelectSample(reservoir, out_sample.x, out_sample.y);
}

void ReservoirSpatialReuse(inout RngStateType rng_state, uint2 coord, PointLightShadingInfo shading_info, float3 view, inout float4 out_sample)
{
    Reservoir reservoir;
    InitReservoir(reservoir);

    float mis_weight = 1.0 / ((2 * spatial_reuse_range + 1) * (2 * spatial_reuse_range + 1));
    for (int x = -spatial_reuse_range; x <= spatial_reuse_range; ++x)
    {
        for (int y = -spatial_reuse_range; y <= spatial_reuse_range; ++y)
        {
            if (x == 0 && y == 0)
            {
                AddReservoirSample(rng_state, reservoir, out_sample.x, mis_weight, luminance(GetLightingByIndex(out_sample.x, shading_info, view)), out_sample.y);
                continue;
            }
            
            int2 spatial_reuse_coord = (int2)coord + int2(x, y);
            if (spatial_reuse_coord.x < 0 || spatial_reuse_coord.y < 0 || spatial_reuse_coord.x >= viewport_width || spatial_reuse_coord.y >= viewport_height)
            {
                continue;
            }
            
            float4 lighting_sample = LightingSamples.Load(int3(spatial_reuse_coord, 0));
            int light_index = round(lighting_sample.x);
            float light_weight = lighting_sample.y;
            if (light_index >= 0 && light_weight > 0.0)
            {
                AddReservoirSample(rng_state, reservoir, light_index, mis_weight, luminance(GetLightingByIndex(light_index, shading_info, view)), light_weight);
            }
        }
    }
    
    GetReservoirSelectSample(reservoir, out_sample.x, out_sample.y);
}

[numthreads(8, 8, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    Output[dispatchThreadID.xy] = 0.0;
    
    uint4 rng = initRNG(dispatchThreadID.xy, uint2(viewport_width, viewport_height), frame_index);
    
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

    float4 final_samples = LightingSamples.Load(int3(dispatchThreadID.xy, 0));
    
    if (enable_temporal_reuse)
    {
        ReservoirTemporalReuse(rng, dispatchThreadID.xy, shading_info, view, final_samples);    
    }
    
    if (enable_spatial_reuse)
    {
        ReservoirSpatialReuse(rng, dispatchThreadID.xy, shading_info, view, final_samples);    
    }
    
    int light_index = final_samples.x;
    float lighting_weight = final_samples.y;
    if (lighting_weight > 0.0 && light_index >= 0)
    {
        float3 final_lighting = lighting_weight * GetLightingByIndex(light_index, shading_info, view);
        Output[dispatchThreadID.xy] = float4(LinearToSrgb(final_lighting), 1.0);
    }
    aggregate_samples_output[dispatchThreadID.xy] = final_samples;
}


#endif