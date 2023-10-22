#ifndef SCENE_MATERIAL
#define SCENE_MATERIAL

#define MATERIAL_TEXTURE_INVALID_INDEX 0xffffffff 
struct MaterialInfo
{
    uint albedo_tex_index;
    uint normal_tex_index;
    uint metallic_roughness_tex_index;
};

StructuredBuffer<MaterialInfo> g_material_infos : SCENE_MATERIAL_INFO_REGISTER_INDEX;
Texture2D<float4> bindless_material_textures[] : SCENE_MATERIAL_TEXTURE_REGISTER_INDEX;
SamplerState material_sampler : SCENE_MATERIAL_SAMPLER_REGISTER_INDEX;

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
    return bindless_material_textures[info.albedo_tex_index].SampleLevel(material_sampler, uv, 0, 0);
}

float4 SampleAlbedoTexture(uint material_id, float2 uv)
{
    MaterialInfo info = g_material_infos[material_id];
    return bindless_material_textures[info.albedo_tex_index].Sample(material_sampler, uv);
}

float4 SampleNormalTextureCS(uint material_id, float2 uv)
{
    MaterialInfo info = g_material_infos[material_id];
    return bindless_material_textures[info.normal_tex_index].SampleLevel(material_sampler, uv, 0, 0);
}

float4 SampleNormalTexture(uint material_id, float2 uv)
{
    MaterialInfo info = g_material_infos[material_id];
    return bindless_material_textures[info.normal_tex_index].Sample(material_sampler, uv);
}

// metalness is sampled in B channel and roughness is sampled in G channel
float2 SampleMetallicRoughnessTexture(uint material_id, float2 uv)
{
    MaterialInfo info = g_material_infos[material_id];
    return bindless_material_textures[info.metallic_roughness_tex_index].Sample(material_sampler, uv).bg;
}

float2 SampleMetallicRoughnessTextureCS(uint material_id, float2 uv)
{
    MaterialInfo info = g_material_infos[material_id];
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
    
    if (material_info.normal_tex_index != MATERIAL_TEXTURE_INVALID_INDEX)
    {
        surface_info.normal = 2 * bindless_material_textures[material_info.normal_tex_index].SampleLevel(material_sampler, uv, 0, 0).xyz - 1.0;
    }
    
    if (material_info.metallic_roughness_tex_index != MATERIAL_TEXTURE_INVALID_INDEX)
    {
        float2 metallic_roughness = bindless_material_textures[material_info.metallic_roughness_tex_index].SampleLevel(material_sampler, uv, 0, 0).bg;
        surface_info.metallic = metallic_roughness.x;
        surface_info.roughness = metallic_roughness.y;
    }
    
    return surface_info;
}

float4 GetMaterialDebugColor(uint material_id)
{
    return material_debug_color[material_id % 8];
}

#endif