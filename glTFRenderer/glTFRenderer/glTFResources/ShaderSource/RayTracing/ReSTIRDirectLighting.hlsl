#ifndef RESTIR_DIRECT_LIGHTING
#define RESTIR_DIRECT_LIGHTING

#include "glTFResources/ShaderSource/FrameStat.hlsl"
#include "glTFResources/ShaderSource/RayTracing/PathTracingRays.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneView.hlsl"
#include "glTFResources/ShaderSource/RayTracing/LightSampling.hlsl"
#include "glTFResources/ShaderSource/ShaderDeclarationUtil.hlsl"

DECLARE_RESOURCE(RaytracingAccelerationStructure scene , SCENE_AS_REGISTER_INDEX);
DECLARE_RESOURCE(RWTexture2D<float4> samples_output , OUTPUT_REGISTER_INDEX);
DECLARE_RESOURCE(RWTexture2D<float4> screen_uv_offset , SCREEN_UV_OFFSET_REGISTER_INDEX);
DECLARE_RESOURCE(RWTexture2D<float4> albedo_output , ALBEDO_REGISTER_INDEX);
DECLARE_RESOURCE(RWTexture2D<float4> normal_output , NORMAL_REGISTER_INDEX);
DECLARE_RESOURCE(RWTexture2D<float> depth_output , DEPTH_REGISTER_INDEX); 

cbuffer RayTracingDIPassOptions: RAY_TRACING_DI_OPTION_CBV_INDEX
{
    bool check_visibility_for_all_candidates;
    int candidate_light_count;
};

[shader("raygeneration")]
void PathTracingRayGen()
{
    samples_output[DispatchRaysIndex().xy] = 0.0;
    screen_uv_offset[DispatchRaysIndex().xy] = -1.0;
    albedo_output[DispatchRaysIndex().xy] = 0.0;
    normal_output[DispatchRaysIndex().xy] = 0.0;
    depth_output[DispatchRaysIndex().xy] = 1.0;
    
    RngStateType rng = initRNG(DispatchRaysIndex().xy, DispatchRaysDimensions().xy, frame_index);
    
    // Add jitter for origin
    float2 jitter_offset = float2 (rand(rng), rand(rng)) - 0.5;
    float2 lerp_values = (DispatchRaysIndex().xy + jitter_offset) / DispatchRaysDimensions().xy;
    
    // Orthographic projection since we're raytracing in screen space.
    float4 ray_offset_dir = mul(inverseProjectionMatrix, float4(2 * lerp_values.x - 1, 1 - 2 * lerp_values.y, 0.0, 1.0));
    float4 ray_world_dir = normalize(mul(inverseViewMatrix, float4(ray_offset_dir.xyz / ray_offset_dir.w, 0.0))); 

    // Trace the ray.
    // Set the ray's extents.
    RayDesc ray;
    ray.Origin = view_position.xyz; 
    ray.Direction = ray_world_dir.xyz;
    // Set TMin to a non-zero small value to avoid aliasing issues due to floating - point errors.
    // TMin should be kept small to prevent missing geometry at close contact areas.
    ray.TMin = 0.001;
    ray.TMax = 10000000.0;
    
    PrimaryRayPayload payload;
    payload.distance = -1.0;
    payload.albedo = 0.0;
    payload.normal = 0.0;
    payload.metallic = 1.0;
    payload.roughness = 1.0;

    TracePrimaryRay(scene, ray, payload);
    if (!IsHit(payload))
    {
        return;
    }
    
    albedo_output[DispatchRaysIndex().xy] = float4(payload.albedo, payload.metallic);
    normal_output[DispatchRaysIndex().xy] = float4(0.5 * payload.normal + 0.5, payload.roughness);
    
    float3 position = ray.Origin + ray.Direction * payload.distance;
    float4 device_coord = mul(projectionMatrix, mul(viewMatrix, float4(position, 1.0)));
    depth_output[DispatchRaysIndex().xy] = device_coord.z / device_coord.w;
        
    float3 view = normalize(view_position.xyz - position);

    // Flip normal if needed
    payload.normal = dot(view, payload.normal) < 0 ? -payload.normal : payload.normal;

    PixelLightingShadingInfo shading_info;
    shading_info.albedo = payload.albedo;
    shading_info.position = position;
    shading_info.normal = payload.normal;
    shading_info.metallic = payload.metallic;
    shading_info.roughness = payload.roughness;
    
    Reservoir sample; InvalidateReservoir(sample);
    SampleLightIndexRIS(rng, candidate_light_count, shading_info, view, check_visibility_for_all_candidates, scene, sample);
    samples_output[DispatchRaysIndex().xy] = PackReservoir(sample);

    float4 ndc_position = mul(prev_projection_matrix, mul(prev_view_matrix, float4(position, 1.0)));
    ndc_position /= ndc_position.w;

    float2 prev_screen_position = float2(ndc_position.x * 0.5 + 0.5, 0.5 - ndc_position.y * 0.5);
    screen_uv_offset[DispatchRaysIndex().xy] = float4(prev_screen_position, 0.0, 0.0);
}

#endif