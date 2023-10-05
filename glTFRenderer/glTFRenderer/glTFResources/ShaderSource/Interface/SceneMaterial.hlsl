#ifndef SCENE_MATERIAL
#define SCENE_MATERIAL

struct MaterialInfo
{
    uint albedo_tex_index;
    uint normal_tex_index;
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

float4 GetMaterialDebugColor(uint material_id)
{
    return material_debug_color[material_id % 8];
}

#endif