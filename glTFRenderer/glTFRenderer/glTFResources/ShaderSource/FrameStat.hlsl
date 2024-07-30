#ifndef FRAME_STAT
#define FRAME_STAT

#include "glTFResources/ShaderSource/ShaderDeclarationUtil.hlsl"

DECLARE_RESOURCE(cbuffer FrameStatConstantBuffer, FRAME_STAT_REGISTER_CBV_INDEX)
{
    uint frame_index;
};

#endif