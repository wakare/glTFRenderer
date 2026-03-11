#ifndef RENDERER_MODULE_LIGHTING
#define RENDERER_MODULE_LIGHTING

#include "../Math/BRDF.hlsl"
#include "BindlessTextureDefine.hlsl"

struct ShadowMapInfo
{
    float4x4 view_matrix;
    float4x4 projection_matrix;
    uint2 shadowmap_size;
    uint vsm_texture_id;
    uint pad;
};
StructuredBuffer<ShadowMapInfo> g_shadowmap_infos;

Texture2D<float> bindless_shadowmap_textures[] : register(t0, BINDLESS_TEXTURE_SPACE_SHADOW);

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

float ComputeDirectionalShadowDepthBias(uint2 shadowmap_extent, float3 scene_normal, float3 light_direction)
{
    const float min_shadowmap_extent = max(min((float)shadowmap_extent.x, (float)shadowmap_extent.y), 1.0f);
    const float ndotl = saturate(dot(normalize(scene_normal), normalize(light_direction)));
    const float constant_bias = 1.5f / min_shadowmap_extent;
    const float slope_bias = (1.0f - ndotl) * (3.0f / min_shadowmap_extent);
    return max(0.00075f, constant_bias + slope_bias);
}

float SampleShadowMapPCF(Texture2D<float> shadowmap, uint2 shadowmap_extent, float2 shadowmap_uv, float shadow_depth, float depth_bias)
{
    const int2 shadowmap_max_texel = int2((int)shadowmap_extent.x - 1, (int)shadowmap_extent.y - 1);
    const float2 shadowmap_texel = shadowmap_uv * float2(shadowmap_extent) - 0.5f;
    const int2 center_texel = int2(floor(shadowmap_texel + 0.5f));

    float visible_sum = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            const int2 sample_texel = clamp(center_texel + int2(x, y), int2(0, 0), shadowmap_max_texel);
            const float compare_shadow_depth = shadowmap.Load(int3(sample_texel, 0));
            visible_sum += shadow_depth <= compare_shadow_depth + depth_bias ? 1.0f : 0.0f;
        }
    }

    return visible_sum / 9.0f;
}

float CalcLightVisibleFactor(uint light_index, float3 scene_position, float3 scene_normal, float3 light_direction)
{
    if (g_lightInfos[light_index].type == 0)
    {
        // directional visible test
        ShadowMapInfo shadowmap_info = g_shadowmap_infos[light_index];
        Texture2D<float> shadowmap = bindless_shadowmap_textures[light_index]; 
        
        float4 shadowmap_ndc = mul(shadowmap_info.projection_matrix, mul(shadowmap_info.view_matrix, float4(scene_position, 1.0)));
        if (abs(shadowmap_ndc.w) < 1e-6f)
        {
            return 1.0;
        }
        shadowmap_ndc /= shadowmap_ndc.w;

        if (shadowmap_ndc.x < -1.0f || shadowmap_ndc.x > 1.0f ||
            shadowmap_ndc.y < -1.0f || shadowmap_ndc.y > 1.0f ||
            shadowmap_ndc.z < 0.0f || shadowmap_ndc.z > 1.0f)
        {
            return 1.0;
        }

        uint2 shadowmap_extent = shadowmap_info.shadowmap_size;
        if (shadowmap_extent.x == 0 || shadowmap_extent.y == 0)
        {
            return 1.0;
        }
        float2 shadowmap_uv = shadowmap_ndc.xy * float2(0.5f, -0.5f) + 0.5f;
        if (shadowmap_uv.x < 0.0f || shadowmap_uv.x > 1.0f ||
            shadowmap_uv.y < 0.0f || shadowmap_uv.y > 1.0f)
        {
            return 1.0;
        }
        const float depth_bias = ComputeDirectionalShadowDepthBias(shadowmap_extent, scene_normal, light_direction);
        return SampleShadowMapPCF(shadowmap, shadowmap_extent, saturate(shadowmap_uv), shadowmap_ndc.z, depth_bias);
    }
    return 1.0;
}

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
        float visible_factor = CalcLightVisibleFactor(sample_light_index, shading_info.position, shading_info.normal, light_vector);
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
