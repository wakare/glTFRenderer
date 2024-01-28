#ifndef PATH_TRACING_MAIN
#define PATH_TRACING_MAIN

#include "glTFResources/ShaderSource/FrameStat.hlsl"
#include "glTFResources/ShaderSource/Math/Sample.hlsl"
#include "glTFResources/ShaderSource/Math/BRDF.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneView.hlsl"
#include "glTFResources/ShaderSource/Lighting/LightingCommon.hlsl"
#include "glTFResources/ShaderSource/RayTracing/PathTracingRays.hlsl"

RaytracingAccelerationStructure scene : SCENE_AS_REGISTER_INDEX;
RWTexture2D<float4> render_target : OUTPUT_REGISTER_INDEX;
RWTexture2D<float4> screen_uv_offset : SCREEN_UV_OFFSET_REGISTER_INDEX;

cbuffer RayTracingPathTracingPassOptions: RAY_TRACING_PATH_TRACING_OPTION_CBV_INDEX
{
    int max_bounce_count;
    int candidate_light_count;
    int samples_per_pixel;
    bool check_visibility_for_all_candidates;
    bool ris_light_sampling;
};

[shader("raygeneration")]
void PathTracingRayGen()
{
    RngStateType rng = initRNG(DispatchRaysIndex().xy, DispatchRaysDimensions().xy, frame_index);
    
    // Add jitter for origin
    float2 jitter_offset = float2 (rand(rng), rand(rng)) - 0.5;
    float2 lerp_values = (DispatchRaysIndex().xy + jitter_offset) / DispatchRaysDimensions().xy;
    
    float3 final_radiance = 0.0;
    float4 pixel_position = 0.0;
    
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
        payload.material_id = 0;
        payload.albedo = 0.0;
        payload.normal = 0.0;
        payload.metallic = 1.0;
        payload.roughness = 1.0;
        
        float3 radiance = 0.0;
        float3 throughput = 1.0;
        
        for (uint i = 0; i < max_bounce_count; ++i)
        {
            TracePrimaryRay(scene, ray, payload);
            if (!IsHit(payload))
            {
                // TODO: Sample skylight info
                radiance += throughput * GetSkylighting();
                break;
            }

            float3 position = ray.Origin + ray.Direction * payload.distance;
            if (i == 0)
            {
                pixel_position = float4(position, 1.0);
            }
            
            float3 view = normalize(view_position.xyz - position);

            // Flip normal if needed
            payload.normal = dot(view, payload.normal) < 0 ? -payload.normal : payload.normal;

            PixelLightingShadingInfo shading_info;
            shading_info.albedo = payload.albedo;
            shading_info.position = position;
            shading_info.normal = payload.normal;
            shading_info.metallic = payload.metallic;
            shading_info.roughness = payload.roughness;

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
                if (SampleLightIndexUniform(rng, sample_light_index, sample_light_weight))
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
            ray.Origin = OffsetRay(position, payload.normal);

            float sample_pdf;
            float3 local_rotation = sampleHemisphere(float2(rand(rng), rand(rng)), sample_pdf);
            ray.Direction = rotatePoint(invertRotation(getRotationToZAxis(payload.normal)), local_rotation);
            
            //throughput /= sample_pdf;
            throughput *= (payload.albedo / sample_pdf);
        }

        final_radiance += radiance;
    }
    
    final_radiance /= samples_per_pixel;
    render_target[DispatchRaysIndex().xy] = float4(LinearToSrgb(final_radiance), 1.0);

    float4 ndc_position = mul(prev_projection_matrix, mul(prev_view_matrix, pixel_position));
    ndc_position /= ndc_position.w;

    float2 prev_screen_position = float2(ndc_position.x * 0.5 + 0.5, 0.5 - ndc_position.y * 0.5);
    screen_uv_offset[DispatchRaysIndex().xy] = float4(prev_screen_position, 0.0, 0.0);
}

#endif // PATH_TRACING_MAIN