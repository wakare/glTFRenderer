#ifndef INTERFACE_LIGHTING
#define INTERFACE_LIGHTING

#include "glTFResources/ShaderSource/Math/RandomGenerator.hlsl"
#include "glTFResources/ShaderSource/Math/Color.hlsl"
#include "glTFResources/ShaderSource/Math/BRDF.hlsl"
#include "glTFResources/ShaderSource/Math/ReservoirSample.hlsl"
#include "glTFResources/ShaderSource/RayTracing/PathTracingRays.hlsl"
#include "glTFResources/ShaderSource/ShaderDeclarationUtil.hlsl"

DECLARE_RESOURCE(cbuffer LightInfoConstantBuffer, SCENE_LIGHT_INFO_CONSTANT_REGISTER_INDEX)
{
    int light_count;
};

struct LightInfo
{
    float3 position;
    float radius;
    
    float3 intensity;
    uint type;
};

struct PixelLightingShadingInfo
{
    float3 position;
    float3 normal;
    float metallic;
    float roughness;

    float3 albedo;
    bool backface;
};

#define LIGHT_TYPE_POINT 0
#define LIGHT_TYPE_DIRECTIONAL 1

DECLARE_RESOURCE(StructuredBuffer<LightInfo> g_lightInfos, SCENE_LIGHT_INFO_REGISTER_INDEX);

float3 GetLightIntensity(uint index, float3 position)
{
    LightInfo info = g_lightInfos[index];
    if (info.type == LIGHT_TYPE_POINT)
    {
        float base_intensity = 1.0 - saturate(length(position - info.position) / info.radius);
        // squared distance falloff 
        return info.intensity * pow(base_intensity, 2.0);
    }
    else if (info.type == LIGHT_TYPE_DIRECTIONAL)
    {
        return info.intensity;
    }
    
    return float3(0.0, 0.0, 0.0);
}

float3 LightingWithPointLight(float3 position, float3 base_color, float3 normal, LightInfo light_info)
{
    float3 point_light_position = light_info.position;
    float3 normalized_light_dir = normalize(point_light_position - position); 
    float geometry_falloff = max(0.0, dot(normal, normalized_light_dir));
    
    float point_light_radius = light_info.radius;
    float point_light_falloff = 2.0;
    float light_intensity = 1.0 - saturate(length(position - point_light_position) / point_light_radius);
    
    return base_color * geometry_falloff * light_info.intensity * pow(light_intensity, point_light_falloff);
}

float3 LightingWithDirectionalLight(float3 position, float3 base_color, float3 normal, LightInfo light_info)
{
    float3 normalized_light_dir = normalize(light_info.position);
    float geometry_falloff = max(0.0, dot(normal, -normalized_light_dir));
    geometry_falloff = pow(geometry_falloff, 2.0);
    
    return base_color * light_info.intensity * geometry_falloff;
}

// return position => light vector with length!
bool GetLightDistanceVector(uint index, float3 position, out float3 light_dir, out float distance)
{
    light_dir = 0.0;
    distance = 0.0;
    
    LightInfo info = g_lightInfos[index];
    if (info.type == LIGHT_TYPE_POINT)
    {
        light_dir = normalize(info.position - position);
        distance = length(info.position - position);
    }
    else if (info.type == LIGHT_TYPE_DIRECTIONAL)
    {
        light_dir = -info.position;
        distance = 10000000.0f;
    }
    else
    {
        return false;
    }
    
    return true;
}

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


float3 GetSkylighting()
{
    //return float3(1.0, 1.0, 1.0);
    return float3(0.0, 0.0, 0.0);
}

float3 GetLightingByIndex(uint sample_light_index, PixelLightingShadingInfo shading_info, float3 view)
{
    float3 light_vector;
    float max_distance;
    if (GetLightDistanceVector(sample_light_index, shading_info.position, light_vector, max_distance))
    {
        // Assume no occluded
        float3 brdf = EvalCookTorranceBRDF(shading_info.normal, shading_info.albedo, shading_info.metallic, shading_info.roughness, view, light_vector);
        return brdf * GetLightIntensity(sample_light_index, shading_info.position) * max(dot(shading_info.normal, light_vector), 0.0);
    }

    return 0.0;
}

float3 GetLightingWithRadiosity(float3 radiance, PixelLightingShadingInfo shading_info)
{
    float3 brdf = EvalDiffuseBRDF(shading_info.albedo);
    return brdf * radiance;
}

float3 GetLighting(PixelLightingShadingInfo shading_info, float3 view)
{
    float3 result = 0.0;
    for (uint i = 0; i < light_count; ++i)
    {
        result += GetLightingByIndex(i, shading_info, view);
    }

    return result;
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