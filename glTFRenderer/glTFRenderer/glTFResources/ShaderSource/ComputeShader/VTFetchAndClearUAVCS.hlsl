#ifndef VT_FETCH_AND_CLEAR_UAVCS_H
#define VT_FETCH_AND_CLEAR_UAVCS_H

#include "glTFResources/ShaderSource/ShaderDeclarationUtil.hlsl"

DECLARE_RESOURCE(RWStructuredBuffer<uint4> fetch_output, VT_FETCH_UAV_OUTPUT_REGISTER_INDEX);

DECLARE_RESOURCE(cbuffer VT_FETCH_OUTPUT_INFO, VT_FETCH_OUTPUT_INFO_CBV_INDEX)
{
    uint texture_count;
    uint texture_width;
    uint texture_height;
};
DECLARE_RESOURCE(RWTexture2D<uint4> clear_uint_textures[] , CLEAR_UINT_TEXTURES_REGISTER_INDEX);

[numthreads(8, 8, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    for (uint i = 0; i < texture_count; i++)
    {
        fetch_output[dispatchThreadID.y * texture_width + dispatchThreadID.x] = clear_uint_textures[i][dispatchThreadID.xy];
        clear_uint_textures[i][dispatchThreadID.xy] = uint4(0, 0, 0, 0);
    }
}

#endif