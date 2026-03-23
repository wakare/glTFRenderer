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

#endif
