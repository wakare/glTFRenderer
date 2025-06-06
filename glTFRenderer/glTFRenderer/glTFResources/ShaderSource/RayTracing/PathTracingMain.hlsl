#ifndef PATH_TRACING_MAIN
#define PATH_TRACING_MAIN

#include "glTFResources/ShaderSource/FrameStat.hlsl"
#include "glTFResources/ShaderSource/Math/Sample.hlsl"
#include "glTFResources/ShaderSource/Math/BRDF.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneView.hlsl"
#include "glTFResources/ShaderSource/RayTracing/LightSampling.hlsl"
#include "glTFResources/ShaderSource/RayTracing/PathTracingRays.hlsl"
#include "glTFResources/ShaderSource/ShaderDeclarationUtil.hlsl"

DECLARE_RESOURCE(RaytracingAccelerationStructure scene , SCENE_AS_REGISTER_INDEX);
DECLARE_RESOURCE(RWTexture2D<float4> render_target , OUTPUT_REGISTER_INDEX);
DECLARE_RESOURCE(RWTexture2D<float4> screen_uv_offset , SCREEN_UV_OFFSET_REGISTER_INDEX);

cbuffer RayTracingPathTracingPassOptions: RAY_TRACING_PATH_TRACING_OPTION_CBV_INDEX
{
    int max_bounce_count;
    int candidate_light_count;
    int samples_per_pixel;
    bool check_visibility_for_all_candidates;
    bool ris_light_sampling;
    bool debug_radiosity;
};

PixelLightingShadingInfo GetShadingInfoFromPayload(PrimaryRayPayload payload, RayDesc ray)
{
    PixelLightingShadingInfo shading_info;
    
    shading_info.backface = dot(ray.Direction, payload.normal) > 0.0;
    shading_info.position = ray.Origin + ray.Direction * payload.distance;
    shading_info.albedo = payload.albedo;
    shading_info.normal = shading_info.backface ? -payload.normal : payload.normal;
    shading_info.metallic = payload.metallic;
    shading_info.roughness = payload.roughness;
    
    return shading_info;
}

[shader("raygeneration")]
void PathTracingRayGen()
{
    RngStateType rng = initRNG(DispatchRaysIndex().xy, DispatchRaysDimensions().xy, frame_index);
    
    // Add jitter for origin
    float2 jitter_offset = float2 (rand(rng), rand(rng)) - 0.5;
    float2 lerp_values = (DispatchRaysIndex().xy + jitter_offset) / DispatchRaysDimensions().xy;
    
    float3 final_radiance = 0.0;
    float4 pixel_position = float4(0.0, 0.0, 0.0, 0.0);

    for (uint path_index = 0; path_index < samples_per_pixel; ++path_index)
    {
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
        
        float3 radiance = 0.0;
        float3 throughput = 1.0;
        {
            for (uint i = 0; i < max_bounce_count; ++i)
            {
                TracePrimaryRay(scene, ray, payload);
                if (!IsHit(payload))
                {
                    // TODO: Sample skylight info
                    radiance += throughput * GetSkylighting();
                    break;
                }

                PixelLightingShadingInfo shading_info = GetShadingInfoFromPayload(payload, ray);
                
                if (i == 0)
                {
                    pixel_position = float4(shading_info.position, 1.0);
                }
            
                float3 view = normalize(view_position.xyz - shading_info.position);
                bool has_valid_light_sampling = false;
                int sample_light_index = -1;
                float sample_light_weight = 0.0;
            
                if (ris_light_sampling)
                {
                    Reservoir sample;
                    if (SampleLightIndexRIS(rng, candidate_light_count, shading_info, view, check_visibility_for_all_candidates, scene, sample))
                    {
                        GetReservoirSelectSample(sample, sample_light_index, sample_light_weight);
                        has_valid_light_sampling = true;
                    }    
                }
                else
                {
                    if (SampleLightIndexUniform(rng, shading_info, scene, check_visibility_for_all_candidates, sample_light_index, sample_light_weight))
                    {
                        has_valid_light_sampling = true;
                    }
                }
            
                if (has_valid_light_sampling)
                {
                    radiance += throughput * sample_light_weight * GetLightingByIndex(sample_light_index, shading_info, view);
                }

                // Russian Roulette
                if (i > 3)
                {
                    float rrProbability = min(0.95f, luminance(throughput));
                    if (rrProbability < rand(rng)) break;
            
                    throughput /= rrProbability;
                }

                // sample material to choose next ray, only diffuse now
                ray.Origin = OffsetRay(shading_info.position, shading_info.normal);

                float sample_pdf;
                //float3 local_rotation = sampleHemisphere(float2(rand(rng), rand(rng)), sample_pdf);
                float3 local_rotation = sampleHemisphereCosine(float2(rand(rng), rand(rng)), sample_pdf);
                ray.Direction = rotatePoint(invertRotation(getRotationToZAxis(shading_info.normal)), local_rotation);
            
                //throughput /= sample_pdf;
                throughput *= (shading_info.albedo / sample_pdf);
            }
        }

        // Avoid too bright radiance influence
        final_radiance += clamp(radiance, 0.0, 1.0);
    }
    
    final_radiance /= samples_per_pixel;
    render_target[DispatchRaysIndex().xy] = float4(LinearToSrgb(final_radiance), 1.0);

    if (pixel_position.w > 0.0)
    {
        float4 ndc_position = mul(prev_projection_matrix, mul(prev_view_matrix, pixel_position));
        ndc_position /= ndc_position.w;

        float2 prev_screen_position = float2(ndc_position.x * 0.5 + 0.5, 0.5 - ndc_position.y * 0.5);
        screen_uv_offset[DispatchRaysIndex().xy] = float4(prev_screen_position, 0.0, 0.0);    
    }
    else
    {
        screen_uv_offset[DispatchRaysIndex().xy] = float4(-1.0, -1.0, 0.0, 0.0); 
    }
}

#endif // PATH_TRACING_MAIN