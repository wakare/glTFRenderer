#include "glTFResources/ShaderSource/MeshPassCommon.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneMaterial.hlsl"
#include "glTFResources/ShaderSource/Interface/VirtualTexture.hlsl"

struct PS_VT_FEEDBACK_OUTPUT
{
    uint4 output: SV_TARGET0;
};

PS_VT_FEEDBACK_OUTPUT main(PS_INPUT input)
{
    MaterialInfo material_info = g_material_infos[input.vs_material_id];
    PS_VT_FEEDBACK_OUTPUT output;

    // TODO: Compress feedback request into 32-bit value to support maximum 4 page request in 1 pixel
    if (material_info.HasVT())
    {
        if (material_info.IsVTAlbedo())
        {
            uint logical_texture_size = g_logical_texture_infos[material_info.albedo_tex_index].logical_texture_size;

            float2 uv = clamp(input.texCoord, 0.0, 0.99);
            float mip = floor(MipLevel(uv, logical_texture_size));
            float max_mip = log2(g_logical_texture_infos[material_info.albedo_tex_index].page_table_texture_size);
            mip = clamp(mip, 0, max_mip);
            float mip_size = logical_texture_size / exp2(mip);
        
            uint2 page_xy = floor(mip_size * uv) / vt_system_info.vt_page_size;
            output.output = uint4( page_xy, mip, material_info.albedo_tex_index);    
        }

        // TODO: Handle other type vt
    }
    
    return output;
}