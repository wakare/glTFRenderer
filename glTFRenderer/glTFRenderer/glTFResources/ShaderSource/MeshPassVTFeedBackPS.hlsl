#include "glTFResources/ShaderSource/MeshPassCommon.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneMaterial.hlsl"

// This function estimates mipmap levels
float MipLevel(float2 uv, float size)
{
    float2 dx = ddx(uv * size);
    float2 dy = ddy(uv * size);
    float d = max(dot(dx, dx), dot(dy, dy));

    return max(0.5 * log2(d), 0);
}

void main(PS_INPUT input)
{
    MaterialInfo material_info = g_material_infos[input.vs_material_id];
    if (material_info.HasVT())
    {
        uint logical_texture_index = g_logical_texture_infos[material_info.albedo_tex_index].logical_texture_output_index;
        uint logical_texture_size = g_logical_texture_infos[material_info.albedo_tex_index].logical_texture_size;

        uint2 page_xy = logical_texture_size * input.texCoord / 64;
        int2 pixelCoord = int2(input.pos.xy);
        bindless_vt_feedback_textures[logical_texture_index][pixelCoord] = uint4( page_xy, MipLevel( input.texCoord, logical_texture_size ), 1);
    }
}