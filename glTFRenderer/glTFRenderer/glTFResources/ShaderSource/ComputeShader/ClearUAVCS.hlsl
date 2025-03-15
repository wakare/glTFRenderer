#ifndef CLEARUAVCS_H
#define CLEARUAVCS_H

#include "glTFResources/ShaderSource/ShaderDeclarationUtil.hlsl"

#ifndef threadBlockSize
#define threadBlockSize 64
#endif

DECLARE_RESOURCE(cbuffer CullingConstant, CLEAR_UAV_INFO_CBV_INDEX)
{
    uint uint_texture_count;
    uint float_texture_count;
};

DECLARE_RESOURCE(RWTexture2D<uint4> clear_uint_textures[] , CLEAR_UINT_TEXTURES_REGISTER_INDEX);
DECLARE_RESOURCE(RWTexture2D<float4> clear_float_textures[] , CLEAR_FLOAT_TEXTURES_REGISTER_INDEX);

[numthreads(8, 8, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    for (uint i = 0; i < uint_texture_count; i++)
    {
        clear_uint_textures[i][dispatchThreadID.xy] = uint4(0, 0, 0, 0); 
    }

    for (uint i = 0; i < float_texture_count; i++)
    {
        clear_float_textures[i][dispatchThreadID.xy] = float4(0, 0, 0, 0);
    }
}

#endif