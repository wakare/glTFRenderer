#ifndef RESERVOIR_SAMPLE
#define RESERVOIR_SAMPLE

#include "glTFResources/ShaderSource/Math/RandomGenerator.hlsl"

struct Reservoir
{
    int select_index;
    float select_target_function_weight;
    float total_weight;
};

void InitReservoir(inout Reservoir reservoir)
{
    reservoir.select_index = -1;
    reservoir.select_target_function_weight = 0.0;
    reservoir.total_weight = 0.0;
}

void AddReservoirSample(inout RngStateType rngState, inout Reservoir reservoir, uint index, float mis_weight, float target_function_weight, float original_weight)
{
    float final_weight = mis_weight * target_function_weight * original_weight;
    reservoir.total_weight += final_weight;
    if (rand(rngState) < (final_weight / reservoir.total_weight))
    {
        reservoir.select_index = index;
        reservoir.select_target_function_weight = target_function_weight;
    }
}

void GetReservoirSelectSample(in Reservoir reservoir, out int out_index, out float out_weight)
{
    out_index = reservoir.select_index;
    out_weight = reservoir.total_weight / max(reservoir.select_target_function_weight, 1e-5);
}

#endif