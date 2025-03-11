#ifndef VIRTUAL_TEXTURE_H
#define VIRTUAL_TEXTURE_H

#include "glTFResources/ShaderSource/ShaderDeclarationUtil.hlsl"

#if VT_READ_DATA

DECLARE_RESOURCE(Texture2D<uint4> bindless_vt_page_table_textures[] , VT_PAGE_TABLE_TEXTURE_REGISTER_INDEX);
DECLARE_RESOURCE(Texture2D<float4> vt_physical_texture, VT_PHYSICAL_TEXTURE_REGISTER_INDEX);

DECLARE_RESOURCE(SamplerState vt_page_table_sampler , VT_PAGE_TABLE_SAMPLER_REGISTER_INDEX);

uint4 SampleVTPageTable(uint tex_id, uint2 uv)
{
    return bindless_vt_page_table_textures[tex_id].Sample(vt_page_table_sampler, uv);
}

#elif VT_FEED_BACK

DECLARE_RESOURCE(RWTexture2D<uint4> bindless_vt_feedback_textures[] , VT_FEED_BACK_TEXTURE_REGISTER_INDEX);

struct VTLogicalTextureInfo
{
    uint logical_texture_output_index;
    uint logical_texture_size;
    uint2 padding;
};
DECLARE_RESOURCE(StructuredBuffer<VTLogicalTextureInfo> g_logical_texture_infos, VT_LOGICAL_TEXTURE_INFO_REGISTER_INDEX);

#endif


#endif