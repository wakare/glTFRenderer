#include "glTFResources/ShaderSource/MeshPassCommon.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneMaterial.hlsl"

void main(PS_INPUT input)
{
    MaterialInfo material_info = g_material_infos[input.vs_material_id];
    
    if (material_info.HasVT())
    {
        uint feed_back_texture_index = g_logical_texture_infos[material_info.albedo_tex_index].feed_back_tex_index;
        uint logical_texture_size = g_logical_texture_infos[material_info.albedo_tex_index].logical_texture_size;

        float2 uv = clamp(input.texCoord, 0.0, 0.99);
        float mip = floor(MipLevel(uv, logical_texture_size));
        mip = clamp(mip, 0, 7);
        float mip_size = logical_texture_size / exp2(mip);
        
        uint2 page_xy = floor(mip_size * uv) / 64.0;
        
        bindless_vt_feedback_textures[feed_back_texture_index][input.pos.xy] = uint4( page_xy, mip, material_info.albedo_tex_index);
    }
}