#ifndef RENDERER_MODULE_LIGHTMAP
#define RENDERER_MODULE_LIGHTMAP

#include "BindlessTextureDefine.hlsl"

#define LIGHTMAP_ATLAS_INVALID_INDEX 0xffffffff

struct LightmapBindingShaderInfo
{
    uint atlas_index;
    uint flags;
    float4 scale_bias;
};

StructuredBuffer<LightmapBindingShaderInfo> g_lightmap_bindings;
Texture2D<float4> bindless_lightmaps[] : register(t0, BINDLESS_TEXTURE_SPACE_LIGHTMAP);

float4 SampleLightmapDiffuseIrradiance(uint lightmap_binding_index, float2 lightmap_uv)
{
    const LightmapBindingShaderInfo binding_info = g_lightmap_bindings[lightmap_binding_index];
    const bool has_lightmap = (binding_info.flags & 0x1u) != 0u &&
                              binding_info.atlas_index != LIGHTMAP_ATLAS_INVALID_INDEX;
    if (!has_lightmap)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    const float2 atlas_uv = saturate(lightmap_uv * binding_info.scale_bias.xy + binding_info.scale_bias.zw);
    const float3 irradiance = bindless_lightmaps[binding_info.atlas_index].SampleLevel(material_sampler, atlas_uv, 0.0f).rgb;
    return float4(irradiance, 1.0f);
}

#endif
