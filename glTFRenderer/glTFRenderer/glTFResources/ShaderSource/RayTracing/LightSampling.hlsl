#ifndef RAY_TRACING_LIGHT_SAMPLING_H
#define RAY_TRACING_LIGHT_SAMPLING_H

#include "glTFResources/ShaderSource/Lighting/LightingCommon.hlsl"
#include "glTFResources/ShaderSource/Math/RandomGenerator.hlsl"
#include "glTFResources/ShaderSource/Math/Color.hlsl"
#include "glTFResources/ShaderSource/Math/ReservoirSample.hlsl"
#include "glTFResources/ShaderSource/RayTracing/PathTracingRays.hlsl"

bool IsLightVisible(uint light_index, PixelLightingShadingInfo shading_info, RaytracingAccelerationStructure scene)
{
    float3 light_vector;
    float light_distance;
    if (GetLightDistanceVector(light_index, shading_info.position, light_vector, light_distance))
    {
        RayDesc visible_ray;
        visible_ray.Origin = OffsetRay(shading_info.position, shading_info.normal);
        visible_ray.Direction = light_vector;
        visible_ray.TMin = 0.0;
        visible_ray.TMax = light_distance;
        return !TraceShadowRay(scene, visible_ray);
    }
    return false;
}

bool SampleLightIndexUniform(inout RngStateType rng_state, PixelLightingShadingInfo shading_info, RaytracingAccelerationStructure scene, bool check_visibility_for_candidates, out uint index, out float weight)
{
    index = 0;
    weight = 0.0;
    
    if (light_count == 0)
    {    
        return false;
    }
    
    index = min(light_count - 1, int((float)light_count * rand(rng_state)));
    weight = light_count;

    return check_visibility_for_candidates ? IsLightVisible(index, shading_info, scene) : true;
}


bool SampleLightIndexRIS(inout RngStateType rngState, uint candidate_count, PixelLightingShadingInfo shading_info, float3 view, bool check_visibility_for_candidates, RaytracingAccelerationStructure scene, out Reservoir sample)
{
    InvalidateReservoir(sample);

    for (uint i = 0; i < candidate_count; ++i)
    {
        uint sample_index;
        float sample_weight;
        if (SampleLightIndexUniform(rngState, shading_info, scene, check_visibility_for_candidates, sample_index, sample_weight))
        {
            AddReservoirSample(rngState, sample, sample_index, 1, 1, luminance(GetLightingByIndex(sample_index, shading_info, view)), sample_weight);
        }
    }

    // If has not checked visibility before, do once check at last
    if (IsReservoirValid(sample) && !check_visibility_for_candidates)
    {
        int index; float weight;
        GetReservoirSelectSample(sample, index, weight);
        
        if (!IsLightVisible(index, shading_info, scene))
        {
            InvalidateReservoir(sample);
        }
    }

    NormalizeReservoir(sample);
    
    return IsReservoirValid(sample);
}
#endif