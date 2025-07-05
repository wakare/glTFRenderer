#ifndef LIGHTING_CS
#define LIGHTING_CS

#include "glTFResources/ShaderSource/Lighting/LightingCommon.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneView.hlsl"
#include "glTFResources/ShaderSource/Math/MathCommon.hlsl"
#include "glTFResources/ShaderSource/ShaderDeclarationUtil.hlsl"

#ifdef USE_VSM
#include "glTFResources/ShaderSource/Interface/VirtualTexture.hlsl"
#endif

DECLARE_RESOURCE(Texture2D albedoTex, ALBEDO_TEX_REGISTER_INDEX);
DECLARE_RESOURCE(Texture2D normalTex, NORMAL_TEX_REGISTER_INDEX);
DECLARE_RESOURCE(Texture2D depthTex, DEPTH_TEX_REGISTER_INDEX);

#ifdef USE_SHADOWMAP
#ifdef USE_VSM

#else
DECLARE_RESOURCE(Texture2D<float> shadowmapTex, SHADOWMAP_TEX_REGISTER_INDEX);
#endif

struct ShadowMapMatrixInfo
{
    float4x4 view_matrix;
    float4x4 projection_matrix;
    uint2 shadowmap_size;
    uint vsm_texture_id;
    uint pad;
};
DECLARE_RESOURCE(StructuredBuffer<ShadowMapMatrixInfo> g_shadowmap_matrix , SHADOW_MAP_MATRIX_INFO_REGISTER_INDEX);

#endif

DECLARE_RESOURCE(RWTexture2D<float4> Output, OUTPUT_TEX_REGISTER_INDEX);
DECLARE_RESOURCE(SamplerState defaultSampler, DEFAULT_SAMPLER_REGISTER_INDEX);

float3 GetWorldPosition(int2 texCoord)
{
    float depth = depthTex.Load(int3(texCoord, 0)).r;

    float2 uv = texCoord / float2(viewport_width - 1, viewport_height - 1);
    
    // Flip uv.y is important
    float4 clipSpaceCoord = float4(2 * uv.x - 1.0, 1 - 2 * uv.y, depth, 1.0);
    float4 viewSpaceCoord = mul(inverseProjectionMatrix, clipSpaceCoord);
    viewSpaceCoord /= viewSpaceCoord.w;

    float4 worldSpaceCoord = mul(inverseViewMatrix, viewSpaceCoord);
    return worldSpaceCoord.xyz;
}

[numthreads(8, 8, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    float3 world_position = GetWorldPosition(dispatchThreadID.xy);

    float4 albedo_buffer_data = albedoTex.Load(int3(dispatchThreadID.xy, 0));
    float4 normal_buffer_data = normalTex.Load(int3(dispatchThreadID.xy, 0));
    
    float3 albedo = albedo_buffer_data.xyz;
    float metallic = albedo_buffer_data.w;
    
    float3 normal = normalize(2 * normal_buffer_data.xyz - 1);
    float roughness = normal_buffer_data.w;

    // Shadowmap
#ifdef USE_SHADOWMAP
    float4 shadowmap_ndc = mul(g_shadowmap_matrix[0].projection_matrix, mul(g_shadowmap_matrix[0].view_matrix, float4(world_position, 1.0)));
    shadowmap_ndc /= shadowmap_ndc.w;
    float2 shadowmap_uv = shadowmap_ndc.xy * 0.5 + 0.5;
    shadowmap_uv.y = 1.0 - shadowmap_uv.y;
    float shadow_depth = shadowmap_ndc.z;
    float compare_shadow_depth = 0.0;
#ifdef USE_VSM
    compare_shadow_depth = SampleVirtualTexture(g_shadowmap_matrix[0].vsm_texture_id, shadowmap_uv).r;
#else
    compare_shadow_depth = shadowmapTex.Load(int3(shadowmap_uv * g_shadowmap_matrix[0].shadowmap_size, 0));
#endif
    
    if (shadow_depth > compare_shadow_depth + 0.01f)
    {
        Output[dispatchThreadID.xy] = float4(0.0, 0.0, 0.0, 1.0);
    }
    else
#endif
    {
        PixelLightingShadingInfo shading_info;
        shading_info.albedo = albedo;
        shading_info.position = world_position;
        shading_info.normal = normal;
        shading_info.metallic = metallic;
        shading_info.roughness = roughness;
        shading_info.backface = false;
    
        float3 view = normalize(view_position.xyz - world_position);
    
        float3 final_lighting = GetLighting(shading_info, view);
    
        Output[dispatchThreadID.xy] = float4(LinearToSrgb(final_lighting), 1.0);    
    }
}

#endif