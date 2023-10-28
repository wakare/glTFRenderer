#ifndef INTERFACE_LIGHTING
#define INTERFACE_LIGHTING

#include "glTFResources/ShaderSource/Math/RandomGenerator.hlsl"
#include "glTFResources/ShaderSource/Math/Color.hlsl"
#include "glTFResources/ShaderSource/Math/BRDF.hlsl"

cbuffer LightInfoConstantBuffer : SCENE_LIGHT_INFO_CONSTANT_REGISTER_INDEX
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

#define LIGHT_TYPE_POINT 0
#define LIGHT_TYPE_DIRECTIONAL 1

StructuredBuffer<LightInfo> g_lightInfos : SCENE_LIGHT_INFO_REGISTER_INDEX;

float3 GetLightIntensity(uint index, float3 position)
{
    LightInfo info = g_lightInfos[index];
    if (info.type == LIGHT_TYPE_POINT)
    {
        float falloff = 1.0 - saturate(length(position - info.position) / info.radius);
        // squared distance falloff 
        return info.intensity * pow(falloff, 2.0);
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

bool SampleLightIndexUniform(inout RngStateType rng_state, out uint index, out float weight)
{
    index = 0;
    weight = 0.0;
    
    if (light_count == 0)
    {    
        return false;
    }
    
    index = min(light_count - 1, uint(light_count * rand(rng_state)));
    weight = light_count;
    
    return true;
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

float3 GetSkylighting()
{
    //return float3(1.0, 1.0, 1.0);
    return float3(0.0, 0.0, 0.0);
}

float3 GetLightingByIndex(uint sample_light_index, float3 position, float3 albedo, float3 normal, float metallic, float roughness, float3 view)
{
    float3 light_vector;
    float max_distance;
    if (GetLightDistanceVector(sample_light_index, position, light_vector, max_distance))
    {
        // Assume no occluded
        float3 brdf = EvalCookTorranceBRDF(normal, view, light_vector, albedo, metallic, roughness);
        return brdf * GetLightIntensity(sample_light_index, position) * max(dot(normal, light_vector), 0.0);
    }

    return 0.0;
}

float3 GetLighting(float3 position, float3 albedo, float3 normal, float metallic, float roughness, float3 view)
{
    float3 result = 0.0;
    for (uint i = 0; i < light_count; ++i)
    {
        result += GetLightingByIndex(i, position, albedo, normal, metallic, roughness, view);
    }

    return result;
}


bool SampleLightIndexRIS(inout RngStateType rngState, uint candidate_count, float3 position, float3 base_color, float3 normal, float metallic, float roughness, float3 view,  out uint index, out float weight)
{
    index = 0;
    weight = 0.0;
    
    float RIS_total_weight = 0.0;
    
    uint sample_index;
    float sample_weight;

    float select_candidate_target = 0.0;
    
    for (uint i = 0; i < candidate_count; ++i)
    {
        if (SampleLightIndexUniform(rngState, sample_index, sample_weight))
        {
            float candidate_target = luminance(GetLightingByIndex(sample_index, position, base_color, normal,  metallic, roughness, view)) ;
            float sample_RIS_weight = candidate_target * sample_weight;
            RIS_total_weight += sample_RIS_weight;

            if (rand(rngState) < (sample_RIS_weight / RIS_total_weight))
            {
                // update current pick info
                index = sample_index;
                select_candidate_target = candidate_target;
            }
        }
    }

    if (RIS_total_weight == 0.0)
    {
        return false;
    }
    
    weight = RIS_total_weight / (candidate_count * select_candidate_target);
    return true;
}

#endif