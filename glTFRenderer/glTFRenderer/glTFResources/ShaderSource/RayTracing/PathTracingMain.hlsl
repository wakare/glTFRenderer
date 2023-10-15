#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#include "glTFResources/ShaderSource/FrameStat.hlsl"
#include "glTFResources/ShaderSource/Math/Sample.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneView.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneMaterial.hlsl"
#include "glTFResources/ShaderSource/Lighting/LightingCommon.hlsl"
#include "glTFResources/ShaderSource/RayTracing/PathTracingRays.hlsl"

RaytracingAccelerationStructure scene : SCENE_AS_REGISTER_INDEX;
RWTexture2D<float4> render_target : OUTPUT_REGISTER_INDEX;

[shader("raygeneration")]
void PathTracingRayGen()
{
    float2 lerpValues = DispatchRaysIndex().xy / DispatchRaysDimensions().xy;
    // Orthographic projection since we're raytracing in screen space.
    float3 rayOffsetDir = float3(lerpValues.x - 0.5, 0.5 - lerpValues.y, 1.0);
    float4 ray_world_dir = normalize(mul(inverseViewMatrix, float4(rayOffsetDir, 0.0))); 

    uint4 rng = initRNG(DispatchRaysIndex().xy, DispatchRaysDimensions().xy, frame_index);
    
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
    payload.uv = float2(0.0, 0.0);
    payload.debug = 0.0;

    float3 radiance = float3(0.0, 0.0, 0.0);
    float3 throughput = float3(1.0, 1.0, 1.0);

    //TracePrimaryRay(scene, ray, payload);
    TraceRay(scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);
    radiance = GetMaterialDebugColor(payload.material_id).xyz;

    /*
    for (uint i = 0; i < 5; ++i)
    {
        TracePrimaryRay(scene, ray, payload);
        if (!IsHit(payload))
        {
            // TODO: Sample skylight info
            radiance += throughput * GetSkylighting();
            break;
        }

        float3 position = ray.Origin + ray.Direction * payload.distance;
        float4 albedo = SampleAlbedoTextureCS(payload.material_id, payload.uv);
        float4 normal = SampleNormalTextureCS(payload.material_id, payload.uv);
        radiance = GetMaterialDebugColor(payload.material_id).xyz;
        //radiance = float3(payload.debug, payload.debug, payload.debug);
        break;
        
        uint sample_light_index;
        float sample_light_weight;
        if (SampleLightIndexRIS(rng, 8, position, albedo.xyz, normal.xyz, sample_light_index, sample_light_weight))
        {
            float3 light_vector = GetLightDistanceVector(sample_light_index, position);
            RayDesc visible_ray;
            visible_ray.Direction = light_vector;
            visible_ray.Origin = position;
            visible_ray.TMin = 0.0;
            visible_ray.TMax = 1.0;
            if (!TraceShadowRay(scene, visible_ray))
            {
                // No occluded
                float3 brdf = albedo.xyz * (1.0 / 3.1415);
                float geometry_falloff = dot(normal.xyz, normalize(GetLightDistanceVector(sample_light_index, position)));
                radiance += throughput * sample_light_weight * brdf * GetLightIntensity(sample_light_index, position) * geometry_falloff;
            }
        }

        // Russian Roulette
        if (i > 2)
        {
            float rrProbability = min(0.95f, luminance(throughput));
            if (rrProbability < rand(rng)) break;
            else throughput /= rrProbability;
        }

        // sample material to choose next ray
        ray.Origin = position;

        float sample_pdf;
        ray.Direction = sampleHemisphere(float2(rand(rng), rand(rng)), sample_pdf);

        throughput /= sample_pdf;
    }
    */
    
    // Write the raytraced color to the output texture.
    render_target[DispatchRaysIndex().xy] = float4(radiance, 1.0);
}

#endif // RAYTRACING_HLSL