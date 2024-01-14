#ifndef RESERVOIR_SAMPLE
#define RESERVOIR_SAMPLE

#include "glTFResources/ShaderSource/Math/RandomGenerator.hlsl"

struct Reservoir
{
    int select_index;
    float select_weight;
    float total_weight;

    void Init()
    {
        select_index = -1;
        select_weight = 0.0;
        total_weight = 0.0;
    }
    
    void AddSample(inout RngStateType rngState, uint index, float weight)
    {
        total_weight += weight;
        if (rand(rngState) < (weight / total_weight))
        {
            select_index = index;
            select_weight = weight;
        }
    }

    void GetSelectSample(out int out_index, out float out_weight)
    {
        out_index = select_index;
        out_weight = total_weight / max(select_weight, 1e-5);
    }
};

#endif