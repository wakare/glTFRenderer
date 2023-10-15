#pragma once

#include "glTFRenderInterfaceSingleConstantBuffer.h"

struct ConstantBufferFrameStat
{
    inline static std::string Name = "FRAME_STAT_REGISTER_CBV_INDEX";
    
    unsigned frame_index;
};

typedef glTFRenderInterfaceSingleConstantBuffer<ConstantBufferFrameStat> glTFRenderInterfaceFrameStat;