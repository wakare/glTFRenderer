#ifndef VIRTUAL_TEXTURE_H
#define VIRTUAL_TEXTURE_H

#include "glTFResources/ShaderSource/ShaderDeclarationUtil.hlsl"

DECLARE_RESOURCE(Texture2D<uint4> bindless_vt_page_table_textures[] , VT_PAGE_TABLE_TEXTURE_REGISTER_INDEX);
DECLARE_RESOURCE(Texture2D<float4> vt_physical_texture, VT_PHYSICAL_TEXTURE_REGISTER_INDEX);

#endif