#ifndef VIRTUAL_TEXTURE_H
#define VIRTUAL_TEXTURE_H

#include "glTFResources/ShaderSource/ShaderDeclarationUtil.hlsl"

struct VTLogicalTextureInfo
{
    uint logical_texture_output_index;
    uint logical_texture_size;
    uint2 padding;
};
DECLARE_RESOURCE(StructuredBuffer<VTLogicalTextureInfo> g_logical_texture_infos, VT_LOGICAL_TEXTURE_INFO_REGISTER_INDEX);

// This function estimates mipmap levels
float MipLevel(float2 uv, float size)
{
    float2 dx = ddx(uv * size);
    float2 dy = ddy(uv * size);
    float d = max(dot(dx, dx), dot(dy, dy));

    return max(0.5 * log2(d), 0);
}

#if VT_READ_DATA

DECLARE_RESOURCE(Texture2D<uint4> bindless_vt_page_table_textures[] , VT_PAGE_TABLE_TEXTURE_REGISTER_INDEX);
DECLARE_RESOURCE(Texture2D<float4> vt_physical_texture, VT_PHYSICAL_TEXTURE_REGISTER_INDEX);

DECLARE_RESOURCE(SamplerState vt_page_table_sampler , VT_PAGE_TABLE_SAMPLER_REGISTER_INDEX);

float4 SampleAtlas(float3 page, float2 uv, float tile_size, float border_size)
{
    const float mipsize = exp2(page.z);

    float page_size = tile_size + 2 * border_size;
    float2 total_offset = fmod(uv * mipsize, tile_size) + float2(border_size, border_size) + page.xy * page_size;  

    uint2 physical_texture_size;
    vt_physical_texture.GetDimensions(physical_texture_size.x, physical_texture_size.y);
    
    return vt_physical_texture.Sample(vt_page_table_sampler, (total_offset) / physical_texture_size);
}

float4 SampleVTPageTable(uint tex_id, float2 uv)
{
    VTLogicalTextureInfo vt_info = g_logical_texture_infos[tex_id];
    float mip = floor(MipLevel(uv, vt_info.logical_texture_size));
    
    uint2 mip_size;
    uint mipLevels;
    bindless_vt_page_table_textures[tex_id].GetDimensions(mip, mip_size.x, mip_size.y, mipLevels);
    
    const float2 uv_without_offset = uv - (frac(uv * mip_size) / mip_size);
    uint4 page = bindless_vt_page_table_textures[tex_id].Load(int3(uv_without_offset * mip_size, 0));

    return SampleAtlas(page.xyz, uv, 64, 1);
}

#elif VT_FEED_BACK

DECLARE_RESOURCE(RWTexture2D<uint4> bindless_vt_feedback_textures[] , VT_FEED_BACK_TEXTURE_REGISTER_INDEX);

#endif


#endif