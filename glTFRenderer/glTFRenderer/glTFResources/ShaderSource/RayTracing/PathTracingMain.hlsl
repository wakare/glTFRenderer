#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#include "glTFResources/ShaderSource/FrameStat.hlsl"
#include "glTFResources/ShaderSource/Math/Sample.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneView.hlsl"
#include "glTFResources/ShaderSource/Lighting/LightingCommon.hlsl"
#include "glTFResources/ShaderSource/RayTracing/PathTracingRays.hlsl"

RaytracingAccelerationStructure scene : SCENE_AS_REGISTER_INDEX;
RWTexture2D<float4> render_target : OUTPUT_REGISTER_INDEX;
RWTexture2D<float4> accumulation_output : ACCUMULATION_OUTPUT_REGISTER_INDEX;

static uint path_sample_count_per_pixel = 1;
static uint path_recursion_depth = 6;

[shader("raygeneration")]
void PathTracingRayGen()
{
    float2 lerpValues = (float2)DispatchRaysIndex().xy / DispatchRaysDimensions().xy;
    uint4 rng = initRNG(DispatchRaysIndex().xy, DispatchRaysDimensions().xy, frame_index);
    
    float3 final_radiance = 0.0;

    for (uint path_index = 0; path_index < path_sample_count_per_pixel; ++path_index)
    {
        // Orthographic projection since we're raytracing in screen space.
        float3 ray_offset_dir = float3(lerpValues.x - 0.5, 0.5 - lerpValues.y, 1.0);
        float4 ray_world_dir = normalize(mul(inverseViewMatrix, float4(ray_offset_dir, 0.0))); 

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
        
        float3 radiance = float3(0.0, 0.0, 0.0);
        float3 throughput = float3(1.0, 1.0, 1.0);
        
        for (uint i = 0; i < path_recursion_depth; ++i)
        {
            TracePrimaryRay(scene, ray, payload);
            if (!IsHit(payload))
            {
                // TODO: Sample skylight info
                radiance += throughput * GetSkylighting();
                break;
            }

            float3 position = ray.Origin + ray.Direction * payload.distance;
            float4 albedo = payload.albedo;
            float4 normal = payload.normal;
        
            uint sample_light_index;
            float sample_light_weight;
            if (SampleLightIndexRIS(rng, 8, position, albedo.xyz, normal.xyz, sample_light_index, sample_light_weight))
            {
                float3 light_vector;
                float max_distance;
                if (GetLightDistanceVector(sample_light_index, position, light_vector, max_distance))
                {
                    RayDesc visible_ray;
                    visible_ray.Direction = light_vector;
                    visible_ray.Origin = position;
                    visible_ray.TMin = 0.000001;
                    visible_ray.TMax = max_distance;
                    
                    if (!TraceShadowRay(scene, visible_ray))
                    {
                        // No occluded
                        float3 brdf = albedo.xyz * (1.0 / 3.1415);
                        float geometry_falloff = max(dot(normal.xyz, normalize(light_vector)), 0.0);
                        radiance += throughput * sample_light_weight * brdf * GetLightIntensity(sample_light_index, position) * geometry_falloff;
                    }
                }
            }

            // Russian Roulette
            if (i > 2)
            {
                float rrProbability = min(0.95f, luminance(throughput));
                if (rrProbability < rand(rng)) break;
            
                throughput /= rrProbability;
            }

            // sample material to choose next ray, only diffuse now
            ray.Origin = position;

            float sample_pdf;
            float3 local_rotation = sampleHemisphere(float2(rand(rng), rand(rng)), sample_pdf);
            ray.Direction = rotatePoint(invertRotation(getRotationToZAxis(normal.xyz)), local_rotation);
            
            //throughput /= sample_pdf;
            throughput *= (albedo.xyz/sample_pdf);
        }

        final_radiance += radiance;
    }

    final_radiance /= path_sample_count_per_pixel;

    uint accumulation_frame_count = (uint)accumulation_output[DispatchRaysIndex().xy].w;
    if (accumulation_frame_count == 0)
    {
        accumulation_output[DispatchRaysIndex().xy] = float4(final_radiance, 1.0);
    }
    else
    {
        float3 old_radiance = accumulation_output[DispatchRaysIndex().xy].xyz / accumulation_frame_count;
        if (length(old_radiance - final_radiance) > 100)
        {
            accumulation_output[DispatchRaysIndex().xy] = float4(final_radiance, 1.0);
        }
        else
        {
            accumulation_output[DispatchRaysIndex().xy] += float4(final_radiance, 1.0);
        }
    }
    
    // Write the raytraced color to the output texture.
    render_target[DispatchRaysIndex().xy] = float4(accumulation_output[DispatchRaysIndex().xy].xyz / accumulation_output[DispatchRaysIndex().xy].w, 1.0);
}

#endif // RAYTRACING_HLSL