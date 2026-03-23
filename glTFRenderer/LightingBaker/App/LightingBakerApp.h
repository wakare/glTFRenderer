#pragma once

#include "BakeJobConfig.h"

#include <string>

namespace LightingBaker
{
    struct BakeOutputLayout;

    class LightingBakerApp
    {
    public:
        int Run(int argc, wchar_t* argv[]);

    private:
        int RunJob(const BakeJobConfig& config);
        void PrintResolvedJob(const BakeJobConfig& config, const BakeOutputLayout& output_layout) const;
    };
}
