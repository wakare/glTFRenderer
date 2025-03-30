#include "glTFResources/ShaderSource/MeshPassCommon.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneMaterial.hlsl"
#include "glTFResources/ShaderSource/Interface/VirtualTexture.hlsl"

struct PS_VT_FEEDBACK_OUTPUT
{
    uint4 output: SV_TARGET0;
};

#ifdef IS_SHADOWMAP_FEEDBACK
DECLARE_RESOURCE(cbuffer ShadowmapInfo, SHADOWMAP_INFO_REGISTER_INDEX)
{
    uint shadowmap_vt_id;
    uint shadowmap_vt_width;
    uint shadowmap_vt_height;
    uint shadowmap_page_table_texture_size;
};
#endif

uint4 GenerateFeedbackData(uint logical_texture_size, uint page_table_texture_size, float2 uv, uint texture_id)
{
    uv = clamp(uv, 0.0, 0.99);
    float mip = floor(MipLevel(uv, logical_texture_size));
    float max_mip = log2(page_table_texture_size);
    mip = clamp(mip, 0, max_mip);
    float mip_size = logical_texture_size / exp2(mip);

    uint2 page_xy = floor(mip_size * uv) / vt_system_info.vt_page_size;
    return uint4( page_xy, mip, texture_id);    
}

PS_VT_FEEDBACK_OUTPUT main(PS_INPUT input)
{
    PS_VT_FEEDBACK_OUTPUT output;
#ifdef IS_SHADOWMAP_FEEDBACK
    output.output = GenerateFeedbackData(shadowmap_vt_width, shadowmap_page_table_texture_size, input.texCoord, shadowmap_vt_id);
    
#else
    MaterialInfo material_info = g_material_infos[input.vs_material_id];
    
    // TODO: Compress feedback request into 32-bit value to support maximum 4 page request in 1 pixel
    if (material_info.HasVT())
    {
        if (material_info.IsVTAlbedo())
        {
            uint logical_texture_size = g_logical_texture_infos[material_info.albedo_tex_index].logical_texture_size;
            uint page_table_texture_size = g_logical_texture_infos[material_info.albedo_tex_index].page_table_texture_size;
            output.output = GenerateFeedbackData(logical_texture_size, page_table_texture_size, input.texCoord, material_info.albedo_tex_index);
        }

        // TODO: Handle other type vt
    }
#endif
    
    return output;
}