#include "glTFResources/ShaderSource/MeshPassCommon.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneMaterial.hlsl"

struct VT_PS_OUTPUT
{
    uint4 FeedBack: SV_TARGET0;
};

// This function estimates mipmap levels
float MipLevel(float2 uv, float size)
{
    float2 dx = ddx(uv * size);
    float2 dy = ddy(uv * size);
    float d = max(dot(dx, dx), dot(dy, dy));

    return max(0.5 * log2(d), 0);
}

VT_PS_OUTPUT main(PS_INPUT input)
{
    VT_PS_OUTPUT output;
    MaterialInfo material_info = g_material_infos[input.vs_material_id];
    if (material_info.IsVTAlbedo())
    {
        float mip = floor( MipLevel( input.texCoord, 8192 ) );    
    }
    else
    {
        output.FeedBack = uint4(0, 0, 0, 0);
    }

    return output;
}