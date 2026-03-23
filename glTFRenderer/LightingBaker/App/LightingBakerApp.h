#pragma once

#include "BakeJobConfig.h"

#include <string>

namespace LightingBaker
{
    struct BakeAccumulationRunResult;
    struct BakeAccumulatorResumeState;
    struct BakeOutputLayout;
    struct BakeRayTracingRuntimeBuildResult;
    struct BakeRayTracingDispatchRunResult;
    struct BakeRayTracingSceneBuildResult;
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
        void PrintRayTracingSceneSummary(const BakeRayTracingSceneBuildResult& ray_tracing_scene) const;
        void PrintRayTracingRuntimeSummary(const BakeRayTracingRuntimeBuildResult& ray_tracing_runtime) const;
        void PrintRayTracingDispatchSummary(const BakeRayTracingDispatchRunResult& ray_tracing_dispatch) const;
        void PrintResumeSummary(const BakeAccumulatorResumeState& resume_state) const;
        void PrintAccumulationSummary(const BakeAccumulationRunResult& run_result) const;
    };
}
