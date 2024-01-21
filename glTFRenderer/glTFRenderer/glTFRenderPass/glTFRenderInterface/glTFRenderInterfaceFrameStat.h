#pragma once

#include "glTFRenderInterfaceSingleConstantBuffer.h"

ALIGN_FOR_CBV_STRUCT struct ConstantBufferFrameStat
{
    inline static std::string Name = "FRAME_STAT_REGISTER_CBV_INDEX";
    
    unsigned frame_index;
};

typedef glTFRenderInterfaceSingleConstantBuffer<ConstantBufferFrameStat> glTFRenderInterfaceFrameStat;