#ifndef RENDERER_MODULE_LIGHTING
#define RENDERER_MODULE_LIGHTING

#include "../Math/BRDF.hlsl"

#ifndef NON_VISIBLE_TEST
#define NON_VISIBLE_TEST
float CalcLightVisibleFactor(uint light_index, float3 scene_position)
{
    return 1.0;
}
#endif

struct LightInfo
{
    float3 position;
    float radius;
    
    float3 intensity;
    uint type; // 0--direction light, 1--point light
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

cbuffer LightInfoConstantBuffer
{
    int light_count;
};
StructuredBuffer<LightInfo> g_lightInfos;

float3 GetLightIntensity(uint index, float3 position)
{
    LightInfo info = g_lightInfos[index];
    if (info.type == 1)
    {
        float base_intensity = 1.0 - saturate(length(position - info.position) / info.radius);
        // squared distance falloff 
        return info.intensity * pow(base_intensity, 2.0);
    }
    else if (info.type == 0)
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
    if (info.type == 1)
    {
        light_dir = normalize(info.position - position);
        distance = length(info.position - position);
    }
    else if (info.type == 0)
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

float3 GetLightingByIndex(uint sample_light_index, PixelLightingShadingInfo shading_info, float3 view)
{
    float3 light_vector;
    float max_distance;
    if (GetLightDistanceVector(sample_light_index, shading_info.position, light_vector, max_distance))
    {
        float visible_factor = CalcLightVisibleFactor(sample_light_index, shading_info.position);
        float3 brdf = EvalCookTorranceBRDF(shading_info.normal, shading_info.albedo, shading_info.metallic, shading_info.roughness, view, light_vector);
        return visible_factor * brdf * GetLightIntensity(sample_light_index, shading_info.position) * max(dot(shading_info.normal, light_vector), 0.0);
    }

    return 0.0;
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

#endif