#ifndef RENDERER_MODULE_MATERIAL
#define RENDERER_MODULE_MATERIAL

#define MATERIAL_TEXTURE_INVALID_INDEX 0xffffffff
struct MaterialShaderInfo
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

StructuredBuffer<MaterialShaderInfo> g_material_infos;
Texture2D<float4> bindless_material_textures[] : register(t0, space1);
SamplerState material_sampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
    AddressW = Clamp;
};
//SamplerState material_sampler;

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

float4 SampleAlbedoTextureCS(uint material_id, float2 uv)
{
    MaterialShaderInfo info = g_material_infos[material_id];

    if (MATERIAL_TEXTURE_INVALID_INDEX == info.albedo_tex_index)
    {
        return info.albedo;
    }
    
    return bindless_material_textures[info.albedo_tex_index].SampleLevel(material_sampler, uv, 0, 0);
}

float4 SampleAlbedoTexture(uint material_id, float2 uv)
{
    MaterialShaderInfo info = g_material_infos[material_id];

    if (MATERIAL_TEXTURE_INVALID_INDEX == info.albedo_tex_index)
    {
        return info.albedo;
    }
    
    return bindless_material_textures[info.albedo_tex_index].Sample(material_sampler, uv);
}

float4 SampleNormalTextureCS(uint material_id, float2 uv)
{
    MaterialShaderInfo info = g_material_infos[material_id];
    
    if (MATERIAL_TEXTURE_INVALID_INDEX == info.normal_tex_index)
    {
        return info.normal;
    }
    
    return bindless_material_textures[info.normal_tex_index].SampleLevel(material_sampler, uv, 0, 0);
}

float4 SampleNormalTexture(uint material_id, float2 uv)
{
    MaterialShaderInfo info = g_material_infos[material_id];
    
    if (MATERIAL_TEXTURE_INVALID_INDEX == info.normal_tex_index)
    {
        return info.normal;
    }
    
    return bindless_material_textures[info.normal_tex_index].Sample(material_sampler, uv);
}

// metalness is sampled in B channel and roughness is sampled in G channel
float2 SampleMetallicRoughnessTexture(uint material_id, float2 uv)
{
    MaterialShaderInfo info = g_material_infos[material_id];
    
    if (MATERIAL_TEXTURE_INVALID_INDEX == info.metallic_roughness_tex_index)
    {
        return info.metallicAndRoughness.bg;
    }
    
    return bindless_material_textures[info.metallic_roughness_tex_index].Sample(material_sampler, uv).bg;
}

float2 SampleMetallicRoughnessTextureCS(uint material_id, float2 uv)
{
    MaterialShaderInfo info = g_material_infos[material_id];
    
    if (MATERIAL_TEXTURE_INVALID_INDEX == info.metallic_roughness_tex_index)
    {
        return info.metallicAndRoughness.bg;
    }
    
    return bindless_material_textures[info.metallic_roughness_tex_index].SampleLevel(material_sampler, uv, 0, 0).bg;
}

#endif