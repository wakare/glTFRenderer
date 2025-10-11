#ifndef RESERVOIR_SAMPLE
#define RESERVOIR_SAMPLE

#include "RandomGenerator.hlsl"

struct Reservoir
{
    int select_index;
    float select_target_function;
    float total_weight;
    int total_count;
};

float4 PackReservoir(Reservoir data)
{
    return float4(float(data.select_index), data.select_target_function, data.total_weight, float(data.total_count));
}

Reservoir UnpackReservoir(float4 data)
{
    Reservoir sample;
    sample.select_index = round(data.x);
    sample.select_target_function = data.y;
    sample.total_weight = data.z;
    sample.total_count = round(data.w);

    return sample;
}

void InvalidateReservoir(out Reservoir reservoir)
{
    reservoir.select_index = -1;
    reservoir.select_target_function = 0.0;
    reservoir.total_weight = 0.0;
    reservoir.total_count = 0;
}

bool IsReservoirValid(Reservoir reservoir)
{
    return reservoir.total_weight > 0.0 && reservoir.total_count > 0 && reservoir.select_index >= 0 && reservoir.select_target_function > 0.0;
}

void NormalizeReservoir(inout Reservoir reservoir)
{
    if (IsReservoirValid(reservoir))
    {
        reservoir.total_weight /= reservoir.total_count;
        reservoir.total_count = 1;    
    }
}

void AddReservoirSample(inout RngStateType rngState, inout Reservoir reservoir, uint index, uint count, float mis_weight, float target_function, float original_weight)
{
    for (uint i = 0; i < count; ++i)
    {
        float final_weight = mis_weight * target_function * original_weight;
        reservoir.total_weight += final_weight;
        ++reservoir.total_count;
        if (rand(rngState) <= (final_weight / reservoir.total_weight))
        {
            reservoir.select_index = index;
            reservoir.select_target_function = target_function;
        }
    }
}

void GetReservoirSelectSample(in Reservoir reservoir, out int out_index, out float out_weight)
{
    out_index = reservoir.select_index;
    out_weight = reservoir.total_weight / (reservoir.total_count * max(reservoir.select_target_function, 1e-5));
}

#endif