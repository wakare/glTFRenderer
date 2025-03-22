#ifndef VIRTUAL_TEXTURE_H
#define VIRTUAL_TEXTURE_H

#include "glTFResources/ShaderSource/ShaderDeclarationUtil.hlsl"

struct VTLogicalTextureInfo
{
    uint feed_back_tex_index;
    uint page_table_tex_index;
    uint logical_texture_size;
    uint page_table_texture_size;
};
DECLARE_RESOURCE(StructuredBuffer<VTLogicalTextureInfo> g_logical_texture_infos, VT_LOGICAL_TEXTURE_INFO_REGISTER_INDEX);

struct VTSystemInfo 
{
    uint vt_page_size;
    uint vt_border_size;
    uint vt_physical_texture_width;
    uint vt_physical_texture_height;
};
DECLARE_RESOURCE(ConstantBuffer<VTSystemInfo> vt_system_info, VT_SYSTEM_REGISTER_CBV_INDEX);

// This function estimates mipmap levels
float MipLevel(float2 uv, float size)
{
    float2 dx = ddx(uv * size);
    float2 dy = ddy(uv * size);
    float d = max(dot(dx, dx), dot(dy, dy));

    return max(0.5 * log2(d), 0);
}

#ifdef VT_READ_DATA

DECLARE_RESOURCE(Texture2D<uint4> bindless_vt_page_table_textures[] , VT_PAGE_TABLE_TEXTURE_REGISTER_INDEX);
DECLARE_RESOURCE(Texture2D<float4> vt_physical_texture, VT_PHYSICAL_TEXTURE_REGISTER_INDEX);

DECLARE_RESOURCE(SamplerState vt_page_table_sampler , VT_PAGE_TABLE_SAMPLER_REGISTER_INDEX);

float4 SampleAtlas(float3 page, float2 uv, float virtual_texture_size, float tile_size, float border_size)
{
    const float mipsize = exp2(log2(virtual_texture_size) - page.z);

    float page_size = tile_size + 2 * border_size;
    float2 total_offset = fmod(uv * mipsize, tile_size) + float2(border_size, border_size) + page.xy * page_size;  

    return vt_physical_texture.Sample(vt_page_table_sampler, (total_offset) / float2(vt_system_info.vt_physical_texture_width, vt_system_info.vt_physical_texture_height));
}

float4 SampleVTPageTable(uint tex_id, float2 uv)
{
    VTLogicalTextureInfo vt_info = g_logical_texture_infos[tex_id];
    float mip = floor(MipLevel(uv, vt_info.logical_texture_size));
    float max_mip = log2(vt_info.page_table_texture_size);
    mip = clamp(mip, 0, max_mip);
    uint mip_size = vt_info.page_table_texture_size >> (uint)mip;
    
    const float2 uv_without_offset = uv - (frac(uv * mip_size) / mip_size);
    uint4 page = bindless_vt_page_table_textures[vt_info.page_table_tex_index].Load(int3(uv_without_offset * mip_size, mip));

    return SampleAtlas(page.xyz, uv, vt_info.logical_texture_size, vt_system_info.vt_page_size, vt_system_info.vt_border_size);
}

#endif


#endif