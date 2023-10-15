#ifndef FRAME_STAT
#define FRAME_STAT

cbuffer FrameStatConstantBuffer : FRAME_STAT_REGISTER_CBV_INDEX
{
    uint frame_index;
};

#endif