#ifndef SCENE_MATERIAL
#define SCENE_MATERIAL

struct MaterialInfo
{
    uint albedo_tex_index;
    uint normal_tex_index;
};

StructuredBuffer<MaterialInfo> g_material_infos : SCENE_MATERIAL_INFO_REGISTER_INDEX;
Texture2D<float4> bindless_material_textures[] : SCENE_MATERIAL_TEXTURE_REGISTER_INDEX;

#endif