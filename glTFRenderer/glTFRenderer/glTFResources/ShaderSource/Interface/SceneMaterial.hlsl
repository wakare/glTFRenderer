#ifndef SCENE_MATERIAL
#define SCENE_MATERIAL

#include "glTFResources/ShaderSource/ShaderDeclarationUtil.hlsl"
#ifdef VT_READ_DATA
#include "glTFResources/ShaderSource/Interface/VirtualTexture.hlsl"
#endif

#define MATERIAL_TEXTURE_INVALID_INDEX 0xffffffff 
struct MaterialInfo
{
    uint albedo_tex_index;
    uint normal_tex_index;
    uint metallic_roughness_tex_index;
    uint virtual_texture_flag;
    
    float4 albedo;
    float4 normal;
    float4 metallicAndRoughness;

    bool IsVTAlbedo() { return virtual_texture_flag & 0x1; }
    bool IsVTNormal() { return virtual_texture_flag & 0x2; }
    bool IsVTMetalicRoughness() { return virtual_texture_flag & 0x4; }
    bool HasVT() { return virtual_texture_flag != 0; }
};

DECLARE_RESOURCE(StructuredBuffer<MaterialInfo> g_material_infos , SCENE_MATERIAL_INFO_REGISTER_INDEX);
DECLARE_RESOURCE(Texture2D<float4> bindless_material_textures[] , SCENE_MATERIAL_TEXTURE_REGISTER_INDEX);
DECLARE_RESOURCE(SamplerState material_sampler , SCENE_MATERIAL_SAMPLER_REGISTER_INDEX);

static float4 material_debug_color[8] =
{
    float4(1.0, 0.0, 0.0, 1.0),
    float4(0.0, 1.0, 0.0, 1.0),
    float4(0.0, 0.0, 1.0, 1.0),
    float4(1.0, 1.0, 0.0, 1.0),
    float4(1.0, 0.0, 1.0, 1.0),
    float4(1.0, 1.0, 1.0, 1.0),
    float4(0.0, 1.0, 1.0, 1.0),
    float4(0.0, 0.0, 0.0, 1.0),
};

struct MaterialSurfaceInfo
{
    float3 albedo;
    float3 normal;
    float metallic;
    float roughness;
};

float4 SampleAlbedoTextureCS(uint material_id, float2 uv)
{
    MaterialInfo info = g_material_infos[material_id];
    if (info.IsVTAlbedo())
    {
#ifdef VT_READ_DATA
        return SampleVTPageTable(info.albedo_tex_index, uv);
#else
        return float4(0.0, 0.0, 0.0, 0.0);
#endif
    }
    if (MATERIAL_TEXTURE_INVALID_INDEX == info.albedo_tex_index)
        return info.albedo;
    
    return bindless_material_textures[info.albedo_tex_index].SampleLevel(material_sampler, uv, 0, 0);
}

float4 SampleAlbedoTexture(uint material_id, float2 uv)
{
    MaterialInfo info = g_material_infos[material_id];

    if (info.IsVTAlbedo())
    {
#ifdef VT_READ_DATA
        return SampleVTPageTable(info.albedo_tex_index, uv);
#else
        return float4(0.0, 0.0, 0.0, 0.0);
#endif
    }
    
    if (MATERIAL_TEXTURE_INVALID_INDEX == info.albedo_tex_index)
    {
        return info.albedo;
    }
    
    return bindless_material_textures[info.albedo_tex_index].Sample(material_sampler, uv);
}

float4 SampleNormalTextureCS(uint material_id, float2 uv)
{
    MaterialInfo info = g_material_infos[material_id];
    if (info.IsVTNormal())
    {
#ifdef VT_READ_DATA
        return SampleVTPageTable(info.normal_tex_index, uv);
#else
        return float4(0.0, 0.0, 0.0, 0.0);
#endif
    }
    
    if (MATERIAL_TEXTURE_INVALID_INDEX == info.normal_tex_index)
    {
        return info.normal;
    }
    
    return bindless_material_textures[info.normal_tex_index].SampleLevel(material_sampler, uv, 0, 0);
}

float4 SampleNormalTexture(uint material_id, float2 uv)
{
    MaterialInfo info = g_material_infos[material_id];
    if (info.IsVTNormal())
    {
#ifdef VT_READ_DATA
        return SampleVTPageTable(info.normal_tex_index, uv);
#else
        return float4(0.0, 0.0, 0.0, 0.0);
#endif
    }
    if (MATERIAL_TEXTURE_INVALID_INDEX == info.normal_tex_index)
    {
        return info.normal;
    }
    
    return bindless_material_textures[info.normal_tex_index].Sample(material_sampler, uv);
}

// metalness is sampled in B channel and roughness is sampled in G channel
float2 SampleMetallicRoughnessTexture(uint material_id, float2 uv)
{
    MaterialInfo info = g_material_infos[material_id];
    if (info.IsVTMetalicRoughness())
    {
#ifdef VT_READ_DATA
        return SampleVTPageTable(info.metallic_roughness_tex_index, uv);
#else
        return float2(0.0, 0.0);
#endif        
    }

    if (MATERIAL_TEXTURE_INVALID_INDEX == info.metallic_roughness_tex_index)
    {
        return info.metallicAndRoughness.bg;
    }
    
    return bindless_material_textures[info.metallic_roughness_tex_index].Sample(material_sampler, uv).bg;
}

float2 SampleMetallicRoughnessTextureCS(uint material_id, float2 uv)
{
    MaterialInfo info = g_material_infos[material_id];
    if (info.IsVTMetalicRoughness())
    {
#ifdef VT_READ_DATA
        return SampleVTPageTable(info.metallic_roughness_tex_index, uv);
#else
        return float2(0.0, 0.0);
#endif        
    }
    if (MATERIAL_TEXTURE_INVALID_INDEX == info.metallic_roughness_tex_index)
    {
        return info.metallicAndRoughness.bg;
    }
    
    return bindless_material_textures[info.metallic_roughness_tex_index].SampleLevel(material_sampler, uv, 0, 0).bg;
}

MaterialSurfaceInfo GetSurfaceInfo(uint material_id, float2 uv)
{
    MaterialInfo material_info = g_material_infos[material_id];
    MaterialSurfaceInfo surface_info =
    {
        1.0, 1.0, 1.0,
        0.0, 1.0, 0.0,
        1.0, 1.0
    };
    
    if (material_info.albedo_tex_index != MATERIAL_TEXTURE_INVALID_INDEX)
    {
        surface_info.albedo = bindless_material_textures[material_info.albedo_tex_index].Sample(material_sampler, uv).xyz;
    }
    
    if (material_info.normal_tex_index != MATERIAL_TEXTURE_INVALID_INDEX)
    {
        surface_info.normal = bindless_material_textures[material_info.normal_tex_index].Sample(material_sampler, uv).xyz;
    }
    
    if (material_info.metallic_roughness_tex_index != MATERIAL_TEXTURE_INVALID_INDEX)
    {
        float2 metallic_roughness = bindless_material_textures[material_info.metallic_roughness_tex_index].Sample(material_sampler, uv).bg;
        surface_info.metallic = metallic_roughness.x;
        surface_info.roughness = metallic_roughness.y;
    }
    
    return surface_info;
}

MaterialSurfaceInfo GetSurfaceInfoCS(uint material_id, float2 uv)
{
    MaterialInfo material_info = g_material_infos[material_id];
    MaterialSurfaceInfo surface_info =
    {
        1.0, 1.0, 1.0,
        0.0, 1.0, 0.0,
        1.0, 1.0
    };
    
    if (material_info.albedo_tex_index != MATERIAL_TEXTURE_INVALID_INDEX)
    {
        surface_info.albedo = bindless_material_textures[material_info.albedo_tex_index].SampleLevel(material_sampler, uv, 0, 0).xyz;
    }
    else
    {
        surface_info.albedo = material_info.albedo.xyz;
    }
    
    if (material_info.normal_tex_index != MATERIAL_TEXTURE_INVALID_INDEX)
    {
        surface_info.normal = 2 * bindless_material_textures[material_info.normal_tex_index].SampleLevel(material_sampler, uv, 0, 0).xyz - 1.0;
    }
    else
    {
        surface_info.normal = material_info.normal.xyz;
    }
    
    if (material_info.metallic_roughness_tex_index != MATERIAL_TEXTURE_INVALID_INDEX)
    {
        float2 metallic_roughness = bindless_material_textures[material_info.metallic_roughness_tex_index].SampleLevel(material_sampler, uv, 0, 0).bg;
        surface_info.metallic = metallic_roughness.x;
        surface_info.roughness = metallic_roughness.y;
    }
    else
    {
        surface_info.metallic = material_info.metallicAndRoughness.b;
        surface_info.roughness = material_info.metallicAndRoughness.g;
    }
    
    return surface_info;
}

float4 GetMaterialDebugColor(uint material_id)
{
    return material_debug_color[material_id % 8];
}

#endif