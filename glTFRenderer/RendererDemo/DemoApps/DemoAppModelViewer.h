#pragma once
#include "DemoBase.h"
#include "Regression/RegressionSuite.h"
#include "RendererCamera.h"
#include "RendererModule/RendererModuleCamera.h"
#include "RendererModule/RendererModuleLighting.h"
#include "RendererSystem/RendererSystemLighting.h"
#include "RendererSystem/RendererSystemSceneRenderer.h"
#include "RendererSystem/RendererSystemSSAO.h"
#include "RendererSystem/RendererSystemToneMap.h"
#include "RendererSystem/RendererSystemTextureDebugView.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class RendererSystemFrostedGlass;

class DemoAppModelViewer : public DemoBase
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DemoAppModelViewer)

protected:
    struct ModelViewerStateSnapshot : NonRenderStateSnapshot
    {
        bool has_camera_pose{false};
        glm::fvec3 camera_position{0.0f};
        glm::fvec3 camera_euler_angles{0.0f};
        unsigned camera_viewport_width{0};
        unsigned camera_viewport_height{0};

        unsigned directional_light_index{0};
        LightInfo directional_light_info{};
        float directional_light_elapsed_seconds{0.0f};
        float directional_light_speed_radians{0.25f};
        bool has_lighting_state{false};
        RendererSystemLighting::LightingGlobalParams lighting_global_params{};
        bool has_tone_map_state{false};
        RendererSystemToneMap::ToneMapGlobalParams tone_map_global_params{};
        bool has_ssao_state{false};
        RendererSystemSSAO::SSAOGlobalParams ssao_global_params{};
        bool has_texture_debug_state{false};
        RendererSystemTextureDebugView::DebugState texture_debug_state{};
    };

    virtual bool InitInternal(const std::vector<std::string>& arguments) override;
    virtual bool RebuildRenderRuntimeObjects() override;
    virtual void TickFrameInternal(unsigned long long time_interval) override;
    virtual void DrawDebugUIInternal() override;
    virtual std::shared_ptr<NonRenderStateSnapshot> CaptureNonRenderStateSnapshot() const override;
    virtual bool ApplyNonRenderStateSnapshot(const std::shared_ptr<NonRenderStateSnapshot>& snapshot) override;
    virtual bool SerializeNonRenderStateSnapshotToJson(
        const std::shared_ptr<NonRenderStateSnapshot>& snapshot,
        nlohmann::json& out_snapshot_json) const override;
    virtual std::shared_ptr<NonRenderStateSnapshot> DeserializeNonRenderStateSnapshotFromJson(
        const nlohmann::json& snapshot_json,
        std::string& out_error) const override;
    virtual const char* GetSnapshotTypeName() const override { return "DemoAppModelViewer"; }

    bool RebuildModelViewerBaseRuntimeObjects();
    bool FinalizeModelViewerRuntimeObjects(const std::shared_ptr<RendererSystemFrostedGlass>& tone_map_frosted_input);
    void UpdateModelViewerFrame(unsigned long long time_interval,
                                bool update_scene_input = true,
                                bool freeze_directional_light = false);
    void DrawModelViewerBaseDebugUI();
    std::shared_ptr<ModelViewerStateSnapshot> CaptureModelViewerStateSnapshot() const;
    bool ApplyModelViewerStateSnapshot(const ModelViewerStateSnapshot& snapshot);
    bool ConfigureRegressionRunFromArguments(const std::vector<std::string>& arguments);
    void TickRegressionAutomation();
    bool ApplyRegressionCaseConfig(const Regression::CaseConfig& case_config, std::string& out_error);
    bool StartRegressionCase(const RendererInterface::RenderGraph::FrameStats& frame_stats);
    bool FinalizeRegressionCase(const RendererInterface::RenderGraph::FrameStats& frame_stats);
    bool FinalizeRegressionRun();
    std::string BuildRegressionCasePrefix(unsigned case_index, const std::string& case_id) const;
    bool CaptureWindowScreenshotPNG(const std::filesystem::path& file_path) const;
    void ResetRegressionPerfAccumulator();
    void AccumulateRegressionPerf(const RendererInterface::RenderGraph::FrameStats& frame_stats);
    bool WriteRegressionPassCsv(const RendererInterface::RenderGraph::FrameStats& frame_stats,
                                const std::filesystem::path& file_path) const;
    bool WriteRegressionPerfJson(const Regression::CaseConfig& case_config,
                                 const RendererInterface::RenderGraph::FrameStats& frame_stats,
                                 const std::filesystem::path& file_path) const;

public:
    struct RegressionCaseResult
    {
        std::string id{};
        bool success{false};
        std::string screenshot_path{};
        std::string pass_csv_path{};
        std::string perf_json_path{};
        std::string error{};
    };

    struct RegressionPerfAccumulator
    {
        struct PassAggregate
        {
            std::string group_name{};
            std::string pass_name{};
            RendererInterface::RenderPassType pass_type{RendererInterface::RenderPassType::GRAPHICS};
            unsigned present_count{0};
            unsigned executed_count{0};
            unsigned skipped_validation_count{0};
            unsigned gpu_valid_count{0};
            double cpu_sum_ms{0.0};
            double gpu_sum_ms{0.0};
        };

        unsigned sample_count{0};
        unsigned gpu_total_valid_count{0};
        unsigned frame_timing_valid_count{0};
        double cpu_total_sum_ms{0.0};
        double gpu_total_sum_ms{0.0};
        double frame_total_sum_ms{0.0};
        double execute_passes_sum_ms{0.0};
        double non_pass_cpu_sum_ms{0.0};
        std::vector<PassAggregate> pass_aggregates{};
    };

protected:
    std::shared_ptr<RendererSystemSceneRenderer> m_scene;
    std::shared_ptr<RendererSystemSSAO> m_ssao;
    std::shared_ptr<RendererSystemLighting> m_lighting;
    std::shared_ptr<RendererSystemToneMap> m_tone_map;
    std::shared_ptr<RendererSystemTextureDebugView> m_texture_debug_view;

    unsigned m_directional_light_index{0};
    LightInfo m_directional_light_info{};
    float m_directional_light_elapsed_seconds{0.0f};
    float m_directional_light_angular_speed_radians{0.25f};
    bool m_regression_enabled{false};
    bool m_regression_finished{false};
    bool m_regression_case_active{false};
    std::filesystem::path m_regression_output_root{};
    Regression::SuiteConfig m_regression_suite{};
    std::shared_ptr<NonRenderStateSnapshot> m_regression_baseline_snapshot{};
    size_t m_regression_case_index{0};
    unsigned long long m_regression_case_start_frame{0};
    unsigned long long m_regression_case_last_elapsed_frames{0};
    RegressionPerfAccumulator m_regression_perf_accumulator{};
    std::vector<RegressionCaseResult> m_regression_case_results{};
    std::string m_regression_last_summary_path{};
};
