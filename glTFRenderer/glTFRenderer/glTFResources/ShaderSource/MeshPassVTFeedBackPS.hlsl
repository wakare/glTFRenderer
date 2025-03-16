#include "glTFResources/ShaderSource/MeshPassCommon.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneMaterial.hlsl"

void main(PS_INPUT input)
{
    MaterialInfo material_info = g_material_infos[input.vs_material_id];
    int2 pixelCoord = int2(input.pos.xy);
    if (material_info.HasVT())
    {
        uint feed_back_texture_index = g_logical_texture_infos[material_info.albedo_tex_index].feed_back_tex_index;
        uint logical_texture_size = g_logical_texture_infos[material_info.albedo_tex_index].logical_texture_size;

        uint2 page_xy = logical_texture_size * clamp(input.texCoord, float2(0.0, 0.0), float2(1.0, 1.0)) / 64;
        
        //bindless_vt_feedback_textures[logical_texture_index][pixelCoord] = uint4( page_xy, floor(MipLevel( input.texCoord, logical_texture_size )), material_info.albedo_tex_index);
        bindless_vt_feedback_textures[feed_back_texture_index][pixelCoord] = uint4( 0,0,7,material_info.albedo_tex_index);
    }
}