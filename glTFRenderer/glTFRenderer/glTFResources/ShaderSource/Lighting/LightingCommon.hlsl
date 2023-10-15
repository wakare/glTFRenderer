#ifndef INTERFACE_LIGHTING
#define INTERFACE_LIGHTING

#include "glTFResources/ShaderSource/Math/RandomGenerator.hlsl"
#include "glTFResources/ShaderSource/Math/Color.hlsl"

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

float3 GetLightingByIndex(uint index, float3 position, float3 base_color, float3 normal)
{
    LightInfo info = g_lightInfos[index];
    if (info.type == LIGHT_TYPE_POINT)
    {
        return LightingWithPointLight(position, base_color, normal, info);
    }
    else if (info.type == LIGHT_TYPE_DIRECTIONAL)
    {
        return LightingWithDirectionalLight(position, base_color, normal, info);
    }
    
    return float3(0.0, 0.0, 0.0);
}

bool SampleLightIndexUniform(inout RngStateType rngState, out uint index, out float weight)
{
    index = 0;
    weight = 0.0;
    
    if (light_count == 0)
    {    
        return false;
    }
    
    index = min(light_count - 1, uint(light_count * rand(rngState)));
    weight = light_count;
    
    return true;
}

bool SampleLightIndexRIS(inout RngStateType rngState, uint candidate_count, float3 position, float3 base_color, float3 normal, out uint index, out float weight)
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
            float candidate_target = luminance(GetLightingByIndex(sample_index, position, base_color, normal)) ;
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

float3 GetLighting(float3 position, float3 base_color, float3 normal)
{
    float3 result = 0.0;
    for (uint i = 0; i < light_count; ++i)
    {
        result += GetLightingByIndex(i, position, base_color, normal);
    }

    return result;
}

// return position => light vector with length!
float3 GetLightDistanceVector(uint index, float3 position)
{
    LightInfo info = g_lightInfos[index];
    if (info.type == LIGHT_TYPE_POINT)
    {
        return info.position - position;
    }
    else if (info.type == LIGHT_TYPE_DIRECTIONAL)
    {
        return -info.position * 1e10;
    }
    
    return float3(0.0, 0.0, 0.0);
}

float3 GetSkylighting()
{
    return float3(0.0, 1.0, 0.5);
}

#endif