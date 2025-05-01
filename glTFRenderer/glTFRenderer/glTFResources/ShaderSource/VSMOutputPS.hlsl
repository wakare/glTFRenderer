#ifndef VSM_OUTPUT_PS_H
#define VSM_OUTPUT_PS_H

#include "glTFResources/ShaderSource/MeshPassCommon.hlsl"

DECLARE_RESOURCE(cbuffer VSMOutputTileOffset, VSM_OUTPUT_FILE_OFFSET_REGISTER_INDEX)
{
    uint offset_x;
    uint offset_y;
    uint width;
    uint height;
};

void main(PS_INPUT input)
{
    float depth = input.pos.z / input.pos.w; // NDC depth
    
    
}

#endif