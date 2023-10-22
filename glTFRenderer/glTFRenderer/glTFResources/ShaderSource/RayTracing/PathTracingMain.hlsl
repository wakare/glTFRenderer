#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#include "glTFResources/ShaderSource/FrameStat.hlsl"
#include "glTFResources/ShaderSource/Math/Sample.hlsl"
#include "glTFResources/ShaderSource/Math/BRDF.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneView.hlsl"
#include "glTFResources/ShaderSource/Lighting/LightingCommon.hlsl"
#include "glTFResources/ShaderSource/RayTracing/PathTracingRays.hlsl"

RaytracingAccelerationStructure scene : SCENE_AS_REGISTER_INDEX;
RWTexture2D<float4> render_target : OUTPUT_REGISTER_INDEX;
RWTexture2D<float4> accumulation_output : ACCUMULATION_OUTPUT_REGISTER_INDEX;
Texture2D<float4> accumulation_backupbuffer : ACCUMULATION_BACKBUFFER_REGISTER_INDEX;

static uint path_sample_count_per_pixel = 1;
static uint path_recursion_depth = 1;
static bool enable_accumulation = true;
static bool srgb_convert = true;

[shader("raygeneration")]
void PathTracingRayGen()
{
    uint4 rng = initRNG(DispatchRaysIndex().xy, DispatchRaysDimensions().xy, frame_index);
    
    // Add jitter for origin
    //float2 jitter_offset = 0.5 * float2 (rand(rng), rand(rng));
    float2 jitter_offset = 0.0;
    float2 lerpValues = (DispatchRaysIndex().xy + jitter_offset) / DispatchRaysDimensions().xy;
    
    float3 final_radiance = 0.0;
    float4 pixel_position = 0.0;
    
    for (uint path_index = 0; path_index < path_sample_count_per_pixel; ++path_index)
    {
        // Orthographic projection since we're raytracing in screen space.
        float4 ray_offset_dir = mul(inverseProjectionMatrix, float4(2 * lerpValues.x - 1, 1 - 2 * lerpValues.y, 0.0, 1.0));
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
            if (i == 0)
            {
                pixel_position = float4(position, 1.0);
            }
            
            float3 view = normalize(view_position.xyz - position);

            // Discard hit backface now...
            if (dot(view, payload.normal) < 0)
            {
                payload.normal = -payload.normal;
                //break;
            }
            
            uint sample_light_index;
            float sample_light_weight;
            if (SampleLightIndexRIS(rng, 8, position, payload.albedo, payload.normal, sample_light_index, sample_light_weight))
            {
                float3 light_vector;
                float max_distance;
                if (GetLightDistanceVector(sample_light_index, position, light_vector, max_distance))
                {
                    RayDesc visible_ray;
                    visible_ray.Origin = OffsetRay(position, payload.normal);
                    visible_ray.Direction = light_vector;
                    visible_ray.TMin = 0.0;
                    visible_ray.TMax = max_distance;
                    
                    if (!TraceShadowRay(scene, visible_ray))
                    {
                        // No occluded
                        float3 brdf = EvalCookTorranceBRDF(payload.normal, view, light_vector, payload.albedo, payload.metallic, payload.roughness);
                        float geometry_falloff = max(dot(payload.normal, light_vector), 0.0);
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
            ray.Origin = OffsetRay(position, payload.normal);

            float sample_pdf;
            float3 local_rotation = sampleHemisphere(float2(rand(rng), rand(rng)), sample_pdf);
            ray.Direction = rotatePoint(invertRotation(getRotationToZAxis(payload.normal)), local_rotation);
            
            //throughput /= sample_pdf;
            throughput *= (payload.albedo / sample_pdf);
        }

        final_radiance += min(radiance, float3(1.0, 1.0, 1.0));
    }
    
    final_radiance /= path_sample_count_per_pixel;

    if (enable_accumulation)
    {        
        float4 ndc_position = mul(prev_projection_matrix, mul(prev_view_matrix, pixel_position));
        ndc_position /= ndc_position.w;
                
        int2 prev_screen_position = 0.5 + float2(ndc_position.x * 0.5 + 0.5, 0.5 - ndc_position.y * 0.5) * float2(viewport_width, viewport_height);
        if (prev_screen_position.x >= 0 && prev_screen_position.x < viewport_width &&
            prev_screen_position.y >= 0 && prev_screen_position.y < viewport_height )
        {
            accumulation_output[DispatchRaysIndex().xy] = accumulation_backupbuffer[prev_screen_position];
        }
        else
        {
            accumulation_output[DispatchRaysIndex().xy] = 0.0;
        }
        
        accumulation_output[DispatchRaysIndex().xy] += float4(final_radiance, 1.0);
        float3 final_color = accumulation_output[DispatchRaysIndex().xy].xyz / accumulation_output[DispatchRaysIndex().xy].w;

        /*
        if (prev_screen_position.x == DispatchRaysIndex().x && prev_screen_position.y == DispatchRaysIndex().y)
        {
            final_color = float3(0.0, 1.0, 0.0);
        }
        */
        
        // Write the raytraced color to the output texture.
        if (srgb_convert)
        {
            final_color = LinearToSrgb(final_color);
        }
        
        render_target[DispatchRaysIndex().xy] = float4(final_color, 1.0);
    }
    else
    {
        if (srgb_convert)
        {
            float3 final_color_srgb = LinearToSrgb(final_radiance);
            render_target[DispatchRaysIndex().xy] = float4(final_color_srgb, 1.0);    
        }
        else
        {
            render_target[DispatchRaysIndex().xy] = float4(final_radiance, 1.0);
        }
    }
}

#endif // RAYTRACING_HLSL