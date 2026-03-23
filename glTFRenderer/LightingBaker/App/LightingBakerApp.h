#pragma once

#include "BakeJobConfig.h"

#include <string>

namespace LightingBaker
{
    struct BakeAccumulationRunResult;
    struct BakeAccumulatorResumeState;
    struct BakeOutputLayout;
    struct BakeSceneImportResult;
    struct LightmapAtlasBuildResult;

    class LightingBakerApp
    {
    public:
        int Run(int argc, wchar_t* argv[]);

    private:
        int RunJob(const BakeJobConfig& config);
        void PrintResolvedJob(const BakeJobConfig& config, const BakeOutputLayout& output_layout) const;
        void PrintImportSummary(const BakeSceneImportResult& import_result) const;
        void PrintAtlasSummary(const LightmapAtlasBuildResult& atlas_result) const;
        void PrintResumeSummary(const BakeAccumulatorResumeState& resume_state) const;
        void PrintAccumulationSummary(const BakeAccumulationRunResult& run_result) const;
    };
}
