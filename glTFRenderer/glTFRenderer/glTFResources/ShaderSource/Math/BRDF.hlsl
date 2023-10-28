#ifndef GLTF_BRDF
#define GLTF_BRDF

#include "glTFResources/ShaderSource/Math/MathCommon.hlsl"

/*
 * Cook Torrance BRDF is composition of (D * F * G) / ((4 * out_dir * normal) * ( input_dir * normal))
 * D - normal distribution function, ratio of surface roughness normal is the same with half vector(L + V)
 * F - fresnel equation function, ratio of specular divide refraction
 * G - geometry function, ratio of macro surface self-occlusion
 */

// Trowbridge-Reitz GGX
float GGX_NDF(float3 normal, float3 half_vector, float roughness)
{
    float roughness_sqr = roughness * roughness;
    float NdotH = max(dot(normal, half_vector), 0.0);
    float tmp = NdotH * NdotH * (roughness_sqr - 1.0) + 1.0;
    
    return roughness_sqr / (PI * tmp * tmp);
}

float3 Schlick_Fresnel(float3 view, float3 half_vector, float3 f0 )
{
    float VdotH = max(0.0, dot(view, half_vector));
    return f0 + (1 - f0) * pow(1 - VdotH, 5.0); 
}

float SchlickGGX_Geometry(float3 normal, float3 view, float k)
{
    float NdotV = max(0.0, dot(normal, view));
    return NdotV / (NdotV * (1 - k) + k);
}

float Smith_Geometry(float3 normal, float3 view, float3 light, float roughness)
{
    // For direct lighting
    float k = (roughness + 1) * (roughness + 1) * (1.0 / 8);

    return SchlickGGX_Geometry(normal, view, k) * SchlickGGX_Geometry(normal, light, k);
}

bool SampleCookTorranceBRDF(out float3 sample_dir, out float sample_pdf)
{
    sample_dir = 0.0;
    sample_pdf = 0.0;
    
    return true;
}

float3 EvalCookTorranceBRDF(float3 normal, float3 view, float3 light, float3 base_color, float metalness, float roughness )
{
    float3 f0 = lerp(float3(0.04, 0.04, 0.04), base_color, metalness);
    float3 half_vector = normalize(light + view); 
    float3 ks = Schlick_Fresnel(view, half_vector, f0);
    float3 kd = 1.0 - ks;

    float3 diffuse = kd * base_color * ONE_OVER_PI;
    float3 specular = ks * GGX_NDF(normal, half_vector, roughness) * Smith_Geometry(normal, view, light, roughness) /
        (4 * max(0.001, dot(view, normal)) * max(0.001, dot(light, normal)));

    return diffuse + specular;
}

#endif