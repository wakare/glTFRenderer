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
Texture2D<float4> environmentTex;
Texture2D<float4> environmentIrradianceTex;
Texture2D<float4> environmentPrefilterTex;
Texture2D<float4> environmentBrdfLutTex;
SamplerState environment_sampler;

cbuffer LightingGlobalBuffer
{
    float4 sky_zenith_color;
    float4 sky_horizon_color;
    float4 ground_color;
    float4 environment_control;
    float4 environment_texture_params;
    float4 environment_prefilter_roughness;
};

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

float3 GetEnvironmentRadiance(float3 sample_direction)
{
    const float3 normalized_direction = normalize(sample_direction);
    const float horizon_exponent = max(environment_control.z, 0.25f);
    float3 radiance = 0.0f;
    if (normalized_direction.y >= 0.0f)
    {
        const float sky_t = pow(saturate(normalized_direction.y), horizon_exponent);
        radiance = lerp(sky_horizon_color.xyz, sky_zenith_color.xyz, sky_t);
    }
    else
    {
        const float ground_t = pow(saturate(-normalized_direction.y), horizon_exponent);
        radiance = lerp(sky_horizon_color.xyz, ground_color.xyz, ground_t);
    }

    if (environment_texture_params.z > 0.5f)
    {
        const float2 latlong_uv = float2(
            atan2(normalized_direction.z, normalized_direction.x) * (0.5f / PI) + 0.5f,
            acos(clamp(normalized_direction.y, -1.0f, 1.0f)) * ONE_OVER_PI);
        const float3 sampled_radiance = environmentTex.SampleLevel(environment_sampler, float2(frac(latlong_uv.x), saturate(latlong_uv.y)), 0.0f).rgb;
        radiance = sampled_radiance * environment_texture_params.x;
    }

    return radiance;
}

float3 SampleEnvironmentIrradiance(float3 sample_direction)
{
    if (environment_texture_params.z <= 0.5f)
    {
        return GetEnvironmentRadiance(sample_direction);
    }

    const float3 normalized_direction = normalize(sample_direction);
    const float2 latlong_uv = float2(
        atan2(normalized_direction.z, normalized_direction.x) * (0.5f / PI) + 0.5f,
        acos(clamp(normalized_direction.y, -1.0f, 1.0f)) * ONE_OVER_PI);
    return environmentIrradianceTex.SampleLevel(
        environment_sampler,
        float2(frac(latlong_uv.x), saturate(latlong_uv.y)),
        0.0f).rgb;
}

float2 GetEnvironmentLatLongUv(float3 sample_direction)
{
    const float3 normalized_direction = normalize(sample_direction);
    return float2(
        atan2(normalized_direction.z, normalized_direction.x) * (0.5f / PI) + 0.5f,
        acos(clamp(normalized_direction.y, -1.0f, 1.0f)) * ONE_OVER_PI);
}

float GetEnvironmentPrefilterMipLod(float roughness, float4 roughness_levels)
{
    if (roughness <= roughness_levels.y)
    {
        return (roughness - roughness_levels.x) / max(roughness_levels.y - roughness_levels.x, 1e-4f);
    }
    if (roughness <= roughness_levels.z)
    {
        const float t = (roughness - roughness_levels.y) / max(roughness_levels.z - roughness_levels.y, 1e-4f);
        return 1.0f + t;
    }

    const float t = (roughness - roughness_levels.z) / max(roughness_levels.w - roughness_levels.z, 1e-4f);
    return 2.0f + saturate(t);
}

float3 SampleEnvironmentPrefilter(float3 sample_direction, float roughness)
{
    if (environment_texture_params.z <= 0.5f)
    {
        return GetEnvironmentRadiance(sample_direction);
    }

    const float clamped_roughness = saturate(roughness);
    const float2 latlong_uv = GetEnvironmentLatLongUv(sample_direction);
    const float2 wrapped_uv = float2(frac(latlong_uv.x), saturate(latlong_uv.y));
    const float4 roughness_levels = environment_prefilter_roughness;

    if (clamped_roughness <= roughness_levels.x)
    {
        const float t = clamped_roughness / max(roughness_levels.x, 1e-4f);
        const float3 sharp_radiance = GetEnvironmentRadiance(sample_direction);
        const float3 first_prefilter_level =
            environmentPrefilterTex.SampleLevel(environment_sampler, wrapped_uv, 0.0f).rgb;
        return lerp(sharp_radiance, first_prefilter_level, t);
    }

    const float mip_lod = GetEnvironmentPrefilterMipLod(clamped_roughness, roughness_levels);
    return environmentPrefilterTex.SampleLevel(environment_sampler, wrapped_uv, mip_lod).rgb;
}

float2 SampleEnvironmentBrdfLut(float ndotv, float roughness)
{
    return environmentBrdfLutTex.SampleLevel(
        environment_sampler,
        float2(saturate(ndotv), saturate(roughness)),
        0.0f).rg;
}

float3 GetEnvironmentDiffuseLighting(PixelLightingShadingInfo shading_info, float ambient_occlusion)
{
    if (environment_control.w <= 0.5f)
    {
        return 0.0f;
    }

    const float3 diffuse_color = shading_info.albedo * (1.0f - shading_info.metallic);
    const float3 environment_irradiance = SampleEnvironmentIrradiance(shading_info.normal);
    return diffuse_color * environment_irradiance * ambient_occlusion * environment_control.x;
}

float3 GetEnvironmentSpecularLighting(PixelLightingShadingInfo shading_info, float3 view)
{
    if (environment_control.w <= 0.5f)
    {
        return 0.0f;
    }

    const float3 reflection_dir = reflect(-view, shading_info.normal);
    const float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), shading_info.albedo, shading_info.metallic);
    const float NdotV = saturate(dot(shading_info.normal, view));
    const float2 integrated_brdf = SampleEnvironmentBrdfLut(NdotV, shading_info.roughness);
    const float3 environment_radiance = SampleEnvironmentPrefilter(reflection_dir, shading_info.roughness);
    const float3 specular_brdf = f0 * integrated_brdf.x + integrated_brdf.y;
    return environment_radiance * specular_brdf * environment_control.y;
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
