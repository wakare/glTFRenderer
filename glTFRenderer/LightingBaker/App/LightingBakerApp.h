#pragma once

#include "BakeJobConfig.h"

#include <string>

namespace LightingBaker
{
    class LightingBakerApp
    {
    public:
        int Run(int argc, wchar_t* argv[]);

    private:
        int RunJob(const BakeJobConfig& config);
        bool EnsureOutputLayout(const BakeJobConfig& config, std::wstring& out_error) const;
        void PrintResolvedJob(const BakeJobConfig& config) const;
    };
}
