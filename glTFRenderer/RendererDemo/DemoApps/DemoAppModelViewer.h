#pragma once
#include "DemoBase.h"
#include "RendererCamera.h"
#include "RendererModule/RendererModuleCamera.h"
#include "RendererModule/RendererModuleLighting.h"
#include "Regression/RegressionSuite.h"
#include "RendererSystem/RendererSystemFrostedGlass.h"
#include "RendererSystem/RendererSystemFrostedPanelProducer.h"
#include "RendererSystem/RendererSystemLighting.h"
#include "RendererSystem/RendererSystemSceneRenderer.h"
#include "RendererSystem/RendererSystemToneMap.h"
#include <filesystem>
#include <string>
#include <vector>

class DemoAppModelViewer : public DemoBase
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DemoAppModelViewer)
    
protected:
    virtual bool InitInternal(const std::vector<std::string>& arguments) override;
    virtual void TickFrameInternal(unsigned long long time_interval) override;
    virtual void DrawDebugUIInternal() override;
    void UpdateFrostedPanelPrepassFeeds(float timeline_seconds);
    bool ConfigureRegressionRunFromArguments(const std::vector<std::string>& arguments);
    void TickRegressionAutomation();
    bool StartRegressionCase(const RendererInterface::RenderGraph::FrameStats& frame_stats);
    bool FinalizeRegressionCase(const RendererInterface::RenderGraph::FrameStats& frame_stats);
    bool FinalizeRegressionRun();
    bool CaptureWindowScreenshotPNG(const std::filesystem::path& file_path) const;
    bool WriteRegressionPassCsv(const RendererInterface::RenderGraph::FrameStats& frame_stats,
                                const std::filesystem::path& file_path) const;
    bool WriteRegressionPerfJson(const RendererInterface::RenderGraph::FrameStats& frame_stats,
                                 const std::filesystem::path& file_path) const;
    bool ApplyRegressionCaseConfig(const Regression::CaseConfig& case_config, std::string& out_error);
    void RefreshImportableRegressionCaseList();
    bool DeleteSelectedImportableRegressionCaseJson();
    bool ImportRegressionCaseFromJson(const std::filesystem::path& suite_path);

    struct RegressionPerfAccumulator
    {
        unsigned sample_count{0};
        unsigned gpu_total_valid_count{0};
        unsigned frosted_gpu_valid_count{0};
        double cpu_total_sum_ms{0.0};
        double gpu_total_sum_ms{0.0};
        double frosted_cpu_sum_ms{0.0};
        double frosted_gpu_sum_ms{0.0};
    };

    struct RegressionCaseResult
    {
        std::string id{};
        bool success{false};
        std::string screenshot_path{};
        std::string pass_csv_path{};
        std::string perf_json_path{};
        std::string error{};
    };

    struct RegressionImportCaseEntry
    {
        std::string file_name{};
        std::filesystem::path full_path{};
    };

    void ResetRegressionPerfAccumulator();
    void AccumulateRegressionPerf(const RendererInterface::RenderGraph::FrameStats& frame_stats);
    bool ExportCurrentRegressionCaseTemplate();

    std::shared_ptr<RendererSystemSceneRenderer> m_scene;
    std::shared_ptr<RendererSystemLighting> m_lighting;
    std::shared_ptr<RendererSystemFrostedPanelProducer> m_frosted_panel_producer;
    std::shared_ptr<RendererSystemFrostedGlass> m_frosted_glass;
    std::shared_ptr<RendererSystemToneMap> m_tone_map;

    unsigned m_directional_light_index{0};
    LightInfo m_directional_light_info{};
    float m_directional_light_elapsed_seconds{0.0f};
    float m_directional_light_angular_speed_radians{0.25f};
    bool m_enable_panel_input_state_machine{true};
    bool m_enable_frosted_prepass_feeds{true};
    std::vector<RendererSystemFrostedPanelProducer::WorldPanelPrepassItem> m_world_prepass_panels{};
    std::vector<RendererSystemFrostedPanelProducer::OverlayPanelPrepassItem> m_overlay_prepass_panels{};
    bool m_regression_enabled{false};
    bool m_regression_finished{false};
    bool m_regression_case_active{false};
    std::filesystem::path m_regression_output_root{};
    Regression::SuiteConfig m_regression_suite{};
    size_t m_regression_case_index{0};
    unsigned long long m_regression_case_start_frame{0};
    unsigned long long m_regression_case_last_elapsed_frames{0};
    RegressionPerfAccumulator m_regression_perf_accumulator{};
    std::vector<RegressionCaseResult> m_regression_case_results{};
    std::string m_regression_last_summary_path{};
    char m_regression_export_case_id[128]{"case_export"};
    std::string m_last_regression_case_export_path{};
    char m_regression_import_directory[260]{"build_logs/regression_case_exports"};
    std::vector<RegressionImportCaseEntry> m_regression_import_case_entries{};
    int m_regression_import_selected_index{-1};
    std::string m_last_regression_case_import_path{};
    std::string m_last_regression_case_import_status{};
};
