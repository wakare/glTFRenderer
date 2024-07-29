#ifndef RESTIR_DIRECT_LIGHTING_CS
#define RESTIR_DIRECT_LIGHTING_CS

#include "glTFResources/ShaderSource/Lighting/LightingCommon.hlsl"
#include "glTFResources/ShaderSource/LightPassCommon.hlsl"
#include "glTFResources/ShaderSource/Math/MathCommon.hlsl"
#include "glTFResources/ShaderSource/FrameStat.hlsl"
#include "glTFResources/ShaderSource/ShaderDeclarationUtil.hlsl"

DECLARE_RESOURCE(Texture2D albedoTex, ALBEDO_REGISTER_INDEX);
DECLARE_RESOURCE(Texture2D normalTex, NORMAL_REGISTER_INDEX);
DECLARE_RESOURCE(Texture2D depthTex, DEPTH_REGISTER_INDEX);

DECLARE_RESOURCE(Texture2D LightingSamples , LIGHTING_SAMPLES_REGISTER_INDEX);
DECLARE_RESOURCE(Texture2D ScreenUVOffset , SCREEN_UV_OFFSET_REGISTER_INDEX);

DECLARE_RESOURCE(RWTexture2D<float4> Output, OUTPUT_TEX_REGISTER_INDEX);

DECLARE_RESOURCE(RWTexture2D<float4> aggregate_samples_output , AGGREGATE_OUTPUT_REGISTER_INDEX);
DECLARE_RESOURCE(Texture2D<float4> aggregate_samples_back_buffer , AGGREGATE_BACKBUFFER_REGISTER_INDEX);

static float spatial_reuse_world_position_threshold = 1;

DECLARE_RESOURCE(cbuffer RayTracingDIPassOptions, RAY_TRACING_DI_POSTPROCESS_OPTION_CBV_INDEX)
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

void ReservoirTemporalReuse(inout RngStateType rng_state, uint2 coord, PixelLightingShadingInfo shading_info, float3 view, float3 world_position, inout Reservoir out_sample)
{
    float2 prev_uv = ScreenUVOffset.Load(int3(coord, 0)).xy;
    uint2 prev_frame_coordination = uint2(round(prev_uv.x * viewport_width), round(prev_uv.y * viewport_height));
    if (prev_frame_coordination.x < 0 || prev_frame_coordination.y < 0 ||
        prev_frame_coordination.x >= viewport_width || prev_frame_coordination.y >= viewport_height)
    {
        return;
    }
    
    float3 prev_normal = DecompressNormalFromTexture(normalTex.Load(int3(prev_frame_coordination, 0)).xyz);
    float3 prev_position = GetWorldPosition(prev_frame_coordination);
    if (0.97 > dot(prev_normal, shading_info.normal) ||
        length(prev_position - world_position) > spatial_reuse_world_position_threshold)
    {
        return;
    }
    
    // Merge current reservoir into old reservoir
    Reservoir reservoir; InvalidateReservoir(reservoir);
    
    if (IsReservoirValid(out_sample))
    {
        int light_index; float light_weight;
        GetReservoirSelectSample(out_sample, light_index, light_weight);
        float target_new = luminance(GetLightingByIndex(light_index, shading_info, view));
        AddReservoirSample(rng_state, reservoir, light_index, 1, 1, target_new, light_weight);
    }
    
    Reservoir old_sample = UnpackReservoir(aggregate_samples_back_buffer.Load(int3(prev_frame_coordination, 0)));
    if (IsReservoirValid(old_sample))
    {
        NormalizeReservoir(old_sample);
        
        int light_index_old; float light_weight_old;
        GetReservoirSelectSample(old_sample, light_index_old, light_weight_old);
        float target_old = luminance(GetLightingByIndex(light_index_old, shading_info, view));
        AddReservoirSample(rng_state, reservoir, light_index_old, 1, 1, target_old, light_weight_old);    
    }

    NormalizeReservoir(reservoir);
    out_sample = reservoir;
}

void ReservoirSpatialReuse(inout RngStateType rng_state, uint2 coord, PixelLightingShadingInfo shading_info, float3 view, float3 world_position, inout Reservoir out_sample)
{
    Reservoir reservoir; InvalidateReservoir(reservoir);

    // TODO: poisson disk sample?
    for (int x = -spatial_reuse_range; x <= spatial_reuse_range; ++x)
    {
        for (int y = -spatial_reuse_range; y <= spatial_reuse_range; ++y)
        {
            int2 spatial_reuse_coord = (int2)coord + int2(x, y);
            if (spatial_reuse_coord.x < 0 || spatial_reuse_coord.y < 0 || spatial_reuse_coord.x >= viewport_width || spatial_reuse_coord.y >= viewport_height)
            {
                continue;
            }
            
            Reservoir sample = UnpackReservoir(LightingSamples.Load(int3(spatial_reuse_coord, 0)));
            if (IsReservoirValid(sample))
            {
                // recalculate spatial weight
                int light_index; float light_weight;
                GetReservoirSelectSample(sample, light_index, light_weight);
 
                float target_weight = luminance(GetLightingByIndex(light_index, shading_info, view));
                AddReservoirSample(rng_state, reservoir, light_index, 1, 1, target_weight, light_weight);
            }
        }
    }
    
    if (IsReservoirValid(out_sample))
    {
        // recalculate spatial weight
        int light_index; float light_weight;
        GetReservoirSelectSample(out_sample, light_index, light_weight);

        float target_weight = luminance(GetLightingByIndex(light_index, shading_info, view));
        AddReservoirSample(rng_state, reservoir, light_index, out_sample.total_count, 1, target_weight, light_weight);
    }
    
    out_sample = reservoir;
}

[numthreads(8, 8, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= viewport_width ||
        dispatchThreadID.y >= viewport_height)
    {
        return;
    }
        
    RngStateType rng = initRNG(dispatchThreadID.xy, uint2(viewport_width, viewport_height), frame_index);
    
    float3 world_position = GetWorldPosition(dispatchThreadID.xy);

    float4 albedo_buffer_data = albedoTex.Load(int3(dispatchThreadID.xy, 0));
    float4 normal_buffer_data = normalTex.Load(int3(dispatchThreadID.xy, 0));

    float3 albedo = albedo_buffer_data.xyz;
    float metallic = albedo_buffer_data.w;
    
    float3 normal = DecompressNormalFromTexture(normal_buffer_data.xyz);
    float roughness = normal_buffer_data.w;

    PixelLightingShadingInfo shading_info;
    shading_info.albedo = albedo;
    shading_info.position = world_position;
    shading_info.normal = normal;
    shading_info.metallic = metallic;
    shading_info.roughness = roughness;
    
    float3 view = normalize(view_position.xyz - world_position);
    
    float4 sample = LightingSamples.Load(int3(dispatchThreadID.xy, 0));
    Reservoir reservoir = UnpackReservoir(sample);
    
    if (enable_temporal_reuse)
    {
        ReservoirTemporalReuse(rng, dispatchThreadID.xy, shading_info, view, world_position, reservoir);    
    }
    
    if (enable_spatial_reuse)
    {
        ReservoirSpatialReuse(rng, dispatchThreadID.xy, shading_info, view, world_position, reservoir);    
    }

    Output[dispatchThreadID.xy] = 0.0;
    if (IsReservoirValid(reservoir))
    {
        int light_index;
        float light_weight;
        GetReservoirSelectSample(reservoir, light_index, light_weight);
        
        float3 final_lighting = light_weight * GetLightingByIndex(light_index, shading_info, view);
        Output[dispatchThreadID.xy] = float4(LinearToSrgb(final_lighting), 1.0);
    }
    
    aggregate_samples_output[dispatchThreadID.xy] = PackReservoir(reservoir);
}


#endif