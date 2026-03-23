#pragma once

namespace LightingBaker
{
    struct BakeAccumulatorSettings
    {
        unsigned samples_per_iteration = 1;
        unsigned target_samples = 256;
        bool progressive = true;
    };

    class BakeAccumulator
    {
    public:
        BakeAccumulator() = default;
    };
}
