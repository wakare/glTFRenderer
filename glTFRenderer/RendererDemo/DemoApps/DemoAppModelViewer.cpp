#include "DemoAppModelViewer.h"

#include "RenderWindow/RendererInputDevice.h"
#include "Regression/RegressionLogicPack.h"
#include "SceneFileLoader/glTFImageIOUtil.h"
#include <glm/glm/gtx/norm.hpp>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <imgui/imgui.h>
#include <sstream>
#include <utility>
#include <windows.h>

namespace
{
    constexpr float DIRECTIONAL_LIGHT_DIRECTION_EPSILON_SQ = 1.0e-8f;

    unsigned long long GetRegressionCaptureEndElapsedFrames(const Regression::CaseConfig& case_config)
    {
        return static_cast<unsigned long long>(case_config.capture.warmup_frames) +
               static_cast<unsigned long long>(case_config.capture.capture_frames);
    }

    const char* ToString(RendererInterface::RenderDeviceType type)
    {
        switch (type)
        {
        case RendererInterface::DX12:
            return "DX12";
        case RendererInterface::VULKAN:
            return "Vulkan";
        }

        return "Unknown";
    }

    const char* ToString(RendererInterface::SwapchainPresentMode mode)
    {
        switch (mode)
        {
        case RendererInterface::SwapchainPresentMode::VSYNC:
            return "VSYNC";
        case RendererInterface::SwapchainPresentMode::MAILBOX:
            return "MAILBOX";
        }

        return "Unknown";
    }

    const char* ToString(RendererInterface::RenderPassType type)
    {
        switch (type)
        {
        case RendererInterface::RenderPassType::GRAPHICS:
            return "Graphics";
        case RendererInterface::RenderPassType::COMPUTE:
            return "Compute";
        case RendererInterface::RenderPassType::RAY_TRACING:
            return "RayTracing";
        }

        return "Unknown";
    }

    DemoAppModelViewer::RegressionPerfAccumulator::PassAggregate& GetOrCreatePassAggregate(
        DemoAppModelViewer::RegressionPerfAccumulator& accumulator,
        const RendererInterface::RenderGraph::RenderPassFrameStats& pass_stat)
    {
        auto it = std::find_if(
            accumulator.pass_aggregates.begin(),
            accumulator.pass_aggregates.end(),
            [&](const DemoAppModelViewer::RegressionPerfAccumulator::PassAggregate& aggregate)
            {
                return aggregate.group_name == pass_stat.group_name &&
                    aggregate.pass_name == pass_stat.pass_name &&
                    aggregate.pass_type == pass_stat.pass_type;
            });

        if (it != accumulator.pass_aggregates.end())
        {
            return *it;
        }

        accumulator.pass_aggregates.push_back({
            .group_name = pass_stat.group_name,
            .pass_name = pass_stat.pass_name,
            .pass_type = pass_stat.pass_type
        });
        return accumulator.pass_aggregates.back();
    }

    std::string SanitizeFileName(std::string value)
    {
        for (char& ch : value)
        {
            if ((ch >= 'a' && ch <= 'z') ||
                (ch >= 'A' && ch <= 'Z') ||
                (ch >= '0' && ch <= '9') ||
                ch == '-' ||
                ch == '_')
            {
                continue;
            }

            ch = '_';
        }

        if (value.empty())
        {
            value = "case";
        }

        return value;
    }

    nlohmann::json ToJson(const glm::fvec3& value)
    {
        return nlohmann::json::array({value.x, value.y, value.z});
    }

    nlohmann::json ToJson(const glm::fvec4& value)
    {
        return nlohmann::json::array({value.x, value.y, value.z, value.w});
    }

    bool ReadVec3(const nlohmann::json& value, glm::fvec3& out_value, std::string& out_error)
    {
        if (!value.is_array() || value.size() != 3u)
        {
            out_error = "expected a 3-element array.";
            return false;
        }
        for (size_t index = 0; index < 3u; ++index)
        {
            if (!value.at(index).is_number())
            {
                out_error = "vector elements must be numbers.";
                return false;
            }
        }
        out_value = glm::fvec3(
            value.at(0).get<float>(),
            value.at(1).get<float>(),
            value.at(2).get<float>());
        return true;
    }

    bool ReadVec4(const nlohmann::json& value, glm::fvec4& out_value, std::string& out_error)
    {
        if (!value.is_array() || value.size() != 4u)
        {
            out_error = "expected a 4-element array.";
            return false;
        }
        for (size_t index = 0; index < 4u; ++index)
        {
            if (!value.at(index).is_number())
            {
                out_error = "vector elements must be numbers.";
                return false;
            }
        }
        out_value = glm::fvec4(
            value.at(0).get<float>(),
            value.at(1).get<float>(),
            value.at(2).get<float>(),
            value.at(3).get<float>());
        return true;
    }
}

void DemoAppModelViewer::UpdateModelViewerFrame(unsigned long long time_interval,
                                                bool update_scene_input,
                                                bool freeze_directional_light)
{
    if (!m_window)
    {
        return;
    }

    auto& input_device = m_window->GetInputDevice();
    if (update_scene_input && m_scene)
    {
        m_scene->UpdateInputDeviceInfo(input_device, time_interval);
    }

    const float delta_seconds = static_cast<float>(time_interval) / 1000.0f;
    if (!freeze_directional_light &&
        std::abs(m_directional_light_angular_speed_radians) > 1.0e-6f)
    {
        m_directional_light_elapsed_seconds += delta_seconds;
    }
    const float rotation_angle = m_directional_light_elapsed_seconds * m_directional_light_angular_speed_radians;

    const glm::vec3 base_direction = glm::normalize(glm::vec3(0.0f, -0.707f, -0.707f));
    const float sin_angle = std::sin(rotation_angle);
    const float cos_angle = std::cos(rotation_angle);
    const glm::vec3 rotated_direction = glm::normalize(glm::vec3(
        cos_angle * base_direction.x + sin_angle * base_direction.z,
        base_direction.y,
        -sin_angle * base_direction.x + cos_angle * base_direction.z));
    if (m_lighting &&
        glm::length2(m_directional_light_info.position - rotated_direction) > DIRECTIONAL_LIGHT_DIRECTION_EPSILON_SQ)
    {
        m_directional_light_info.position = rotated_direction;
        m_lighting->UpdateLight(m_directional_light_index, m_directional_light_info);
    }
}

std::shared_ptr<DemoAppModelViewer::ModelViewerStateSnapshot> DemoAppModelViewer::CaptureModelViewerStateSnapshot() const
{
    auto snapshot = std::make_shared<ModelViewerStateSnapshot>();
    snapshot->directional_light_index = m_directional_light_index;
    snapshot->directional_light_info = m_directional_light_info;
    snapshot->directional_light_elapsed_seconds = m_directional_light_elapsed_seconds;
    snapshot->directional_light_speed_radians = m_directional_light_angular_speed_radians;

    if (m_scene)
    {
        const auto camera_module = m_scene->GetCameraModule();
        if (camera_module)
        {
            snapshot->has_camera_pose = camera_module->GetCameraPose(
                snapshot->camera_position,
                snapshot->camera_euler_angles);
            snapshot->camera_viewport_width = camera_module->GetWidth();
            snapshot->camera_viewport_height = camera_module->GetHeight();
        }
    }

    if (m_lighting)
    {
        snapshot->has_lighting_state = true;
        snapshot->lighting_global_params = m_lighting->GetGlobalParams();
    }
    if (m_tone_map)
    {
        snapshot->has_tone_map_state = true;
        snapshot->tone_map_global_params = m_tone_map->GetGlobalParams();
    }
    if (m_ssao)
    {
        snapshot->has_ssao_state = true;
        snapshot->ssao_global_params = m_ssao->GetGlobalParams();
    }
    if (m_texture_debug_view)
    {
        snapshot->has_texture_debug_state = true;
        snapshot->texture_debug_state = m_texture_debug_view->GetDebugState();
    }

    return snapshot;
}

bool DemoAppModelViewer::ConfigureRegressionRunFromArguments(const std::vector<std::string>& arguments)
{
    std::string suite_path_arg{};
    std::string output_root_arg{};
    bool enable_regression = false;

    const std::string suite_prefix = "-regression-suite=";
    const std::string output_prefix = "-regression-output=";
    for (const auto& argument : arguments)
    {
        if (argument == "-regression")
        {
            enable_regression = true;
            continue;
        }
        if (argument.rfind(suite_prefix, 0) == 0)
        {
            enable_regression = true;
            suite_path_arg = argument.substr(suite_prefix.length());
            continue;
        }
        if (argument.rfind(output_prefix, 0) == 0)
        {
            output_root_arg = argument.substr(output_prefix.length());
            continue;
        }
    }

    if (!enable_regression)
    {
        m_regression_baseline_snapshot.reset();
        return true;
    }
    if (suite_path_arg.empty())
    {
        LOG_FORMAT_FLUSH("[Regression] Missing -regression-suite=<path> for DemoAppModelViewer.\n");
        return false;
    }

    std::string load_error{};
    if (!Regression::LoadSuiteConfig(suite_path_arg, m_regression_suite, load_error))
    {
        LOG_FORMAT_FLUSH("[Regression] Failed to load suite '%s': %s\n", suite_path_arg.c_str(), load_error.c_str());
        return false;
    }

    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm{};
    localtime_s(&local_tm, &now_time);
    std::ostringstream stamp_stream;
    stamp_stream << std::put_time(&local_tm, "%Y%m%d_%H%M%S");
    const std::string stamp = stamp_stream.str();

    std::filesystem::path root_dir = output_root_arg.empty()
        ? (std::filesystem::path("build_logs") / "regression")
        : std::filesystem::path(output_root_arg);
    m_regression_output_root = root_dir / (SanitizeFileName(m_regression_suite.suite_name) + "_" + stamp);

    std::error_code fs_error;
    std::filesystem::create_directories(m_regression_output_root / "cases", fs_error);
    if (fs_error)
    {
        LOG_FORMAT_FLUSH("[Regression] Failed to create output directory: %s\n",
                         m_regression_output_root.string().c_str());
        return false;
    }
    m_regression_baseline_snapshot = CaptureNonRenderStateSnapshot();
    if (!m_regression_baseline_snapshot)
    {
        LOG_FORMAT_FLUSH("[Regression] Failed to capture baseline state snapshot for suite '%s'.\n",
                         m_regression_suite.suite_name.c_str());
        return false;
    }

    m_regression_enabled = true;
    m_regression_finished = false;
    m_regression_case_active = false;
    m_regression_case_index = 0;
    m_regression_case_start_frame = 0;
    m_regression_case_last_elapsed_frames = 0;
    m_regression_case_results.clear();
    m_regression_last_summary_path.clear();
    ResetRegressionPerfAccumulator();

    if (m_regression_suite.disable_debug_ui && m_render_graph)
    {
        m_render_graph->EnableDebugUI(false);
    }

    LOG_FORMAT_FLUSH("[Regression] Loaded suite '%s' with %u case(s). Output: %s\n",
                     m_regression_suite.suite_name.c_str(),
                     static_cast<unsigned>(m_regression_suite.cases.size()),
                     m_regression_output_root.string().c_str());
    return true;
}

void DemoAppModelViewer::ResetRegressionPerfAccumulator()
{
    m_regression_perf_accumulator = RegressionPerfAccumulator{};
}

void DemoAppModelViewer::AccumulateRegressionPerf(const RendererInterface::RenderGraph::FrameStats& frame_stats)
{
    m_regression_perf_accumulator.sample_count += 1u;
    m_regression_perf_accumulator.cpu_total_sum_ms += frame_stats.cpu_total_ms;
    if (frame_stats.gpu_time_valid)
    {
        m_regression_perf_accumulator.gpu_total_valid_count += 1u;
        m_regression_perf_accumulator.gpu_total_sum_ms += frame_stats.gpu_total_ms;
    }

    for (const auto& pass_stat : frame_stats.pass_stats)
    {
        auto& pass_aggregate = GetOrCreatePassAggregate(m_regression_perf_accumulator, pass_stat);
        pass_aggregate.present_count += 1u;
        pass_aggregate.executed_count += pass_stat.executed ? 1u : 0u;
        pass_aggregate.skipped_validation_count += pass_stat.skipped_due_to_validation ? 1u : 0u;
        pass_aggregate.gpu_valid_count += pass_stat.gpu_time_valid ? 1u : 0u;
        pass_aggregate.cpu_sum_ms += pass_stat.cpu_time_ms;
        pass_aggregate.gpu_sum_ms += pass_stat.gpu_time_ms;
    }

    if (m_render_graph)
    {
        const auto& frame_timing = m_render_graph->GetLastFrameTimingBreakdown();
        if (frame_timing.valid)
        {
            m_regression_perf_accumulator.frame_timing_valid_count += 1u;
            m_regression_perf_accumulator.frame_total_sum_ms += frame_timing.frame_total_ms;
            m_regression_perf_accumulator.execute_passes_sum_ms += frame_timing.execute_passes_ms;
            m_regression_perf_accumulator.non_pass_cpu_sum_ms += frame_timing.non_pass_cpu_ms;
        }
    }
}

bool DemoAppModelViewer::WriteRegressionPassCsv(const RendererInterface::RenderGraph::FrameStats& frame_stats,
                                                const std::filesystem::path& file_path) const
{
    const unsigned sample_count = m_regression_perf_accumulator.sample_count;
    if (sample_count == 0u)
    {
        return false;
    }

    std::ofstream csv_stream(file_path, std::ios::out | std::ios::trunc);
    if (!csv_stream.is_open())
    {
        return false;
    }

    csv_stream << "capture_end_frame_index,sample_count,group,pass,pass_type,present_ratio,executed_ratio,skipped_validation_ratio,cpu_avg_ms,gpu_valid_ratio,gpu_avg_ms\n";
    for (const auto& pass_aggregate : m_regression_perf_accumulator.pass_aggregates)
    {
        csv_stream << frame_stats.frame_index << ","
                   << sample_count << ","
                   << "\"" << pass_aggregate.group_name << "\","
                   << "\"" << pass_aggregate.pass_name << "\","
                   << "\"" << ToString(pass_aggregate.pass_type) << "\","
                   << std::fixed << std::setprecision(4) << (static_cast<double>(pass_aggregate.present_count) / static_cast<double>(sample_count)) << ","
                   << std::fixed << std::setprecision(4) << (static_cast<double>(pass_aggregate.executed_count) / static_cast<double>(sample_count)) << ","
                   << std::fixed << std::setprecision(4) << (static_cast<double>(pass_aggregate.skipped_validation_count) / static_cast<double>(sample_count)) << ","
                   << std::fixed << std::setprecision(4) << (pass_aggregate.cpu_sum_ms / static_cast<double>(sample_count)) << ","
                   << std::fixed << std::setprecision(4) << (static_cast<double>(pass_aggregate.gpu_valid_count) / static_cast<double>(sample_count)) << ","
                   << std::fixed << std::setprecision(4) << (pass_aggregate.gpu_sum_ms / static_cast<double>(sample_count)) << "\n";
    }

    return true;
}

bool DemoAppModelViewer::WriteRegressionPerfJson(const Regression::CaseConfig& case_config,
                                                 const RendererInterface::RenderGraph::FrameStats& frame_stats,
                                                 const std::filesystem::path& file_path) const
{
    const auto safe_avg = [](double sum, unsigned count) -> double
    {
        return count > 0u ? (sum / static_cast<double>(count)) : 0.0;
    };

    nlohmann::json summary{};
    summary["suite_name"] = m_regression_suite.suite_name;
    summary["case_id"] = case_config.id;
    summary["render_device"] = ToString(m_render_device_type);
    summary["present_mode"] = ToString(
        m_resource_manager ? m_resource_manager->GetSwapchainPresentMode() : m_swapchain_present_mode_ui);
    summary["capture_frame_index"] = frame_stats.frame_index;
    summary["sample_count"] = m_regression_perf_accumulator.sample_count;
    summary["cpu_total_avg_ms"] = safe_avg(
        m_regression_perf_accumulator.cpu_total_sum_ms,
        m_regression_perf_accumulator.sample_count);
    if (m_regression_perf_accumulator.gpu_total_valid_count > 0u)
    {
        summary["gpu_total_avg_ms"] = safe_avg(
            m_regression_perf_accumulator.gpu_total_sum_ms,
            m_regression_perf_accumulator.gpu_total_valid_count);
    }
    else
    {
        summary["gpu_total_avg_ms"] = nullptr;
    }
    summary["frosted_cpu_avg_ms"] = nullptr;
    summary["frosted_gpu_avg_ms"] = nullptr;

    if (m_regression_perf_accumulator.frame_timing_valid_count > 0u)
    {
        const unsigned timing_count = m_regression_perf_accumulator.frame_timing_valid_count;
        summary["frame_total_avg_ms"] = safe_avg(m_regression_perf_accumulator.frame_total_sum_ms, timing_count);
        summary["execute_passes_avg_ms"] = safe_avg(m_regression_perf_accumulator.execute_passes_sum_ms, timing_count);
        summary["non_pass_cpu_avg_ms"] = safe_avg(m_regression_perf_accumulator.non_pass_cpu_sum_ms, timing_count);
    }
    else
    {
        summary["frame_total_avg_ms"] = nullptr;
        summary["execute_passes_avg_ms"] = nullptr;
        summary["non_pass_cpu_avg_ms"] = nullptr;
    }

    summary["pass_stats_avg"] = nlohmann::json::array();
    if (m_regression_perf_accumulator.sample_count > 0u)
    {
        const double sample_count_double = static_cast<double>(m_regression_perf_accumulator.sample_count);
        for (const auto& pass_aggregate : m_regression_perf_accumulator.pass_aggregates)
        {
            summary["pass_stats_avg"].push_back({
                {"group", pass_aggregate.group_name},
                {"pass", pass_aggregate.pass_name},
                {"pass_type", ToString(pass_aggregate.pass_type)},
                {"present_ratio", static_cast<double>(pass_aggregate.present_count) / sample_count_double},
                {"executed_ratio", static_cast<double>(pass_aggregate.executed_count) / sample_count_double},
                {"skipped_validation_ratio", static_cast<double>(pass_aggregate.skipped_validation_count) / sample_count_double},
                {"cpu_avg_ms", pass_aggregate.cpu_sum_ms / sample_count_double},
                {"gpu_valid_ratio", static_cast<double>(pass_aggregate.gpu_valid_count) / sample_count_double},
                {"gpu_avg_ms", pass_aggregate.gpu_sum_ms / sample_count_double}
            });
        }
    }

    std::ofstream json_stream(file_path, std::ios::out | std::ios::trunc);
    if (!json_stream.is_open())
    {
        return false;
    }
    json_stream << summary.dump(2) << "\n";
    return true;
}

std::string DemoAppModelViewer::BuildRegressionCasePrefix(unsigned case_index, const std::string& case_id) const
{
    std::ostringstream case_prefix_stream;
    case_prefix_stream << std::setfill('0') << std::setw(3) << case_index << "_" << SanitizeFileName(case_id);
    return case_prefix_stream.str();
}

bool DemoAppModelViewer::ApplyRegressionCaseConfig(const Regression::CaseConfig& case_config, std::string& out_error)
{
    out_error.clear();

    if (case_config.camera.enabled)
    {
        const auto camera_module = m_scene ? m_scene->GetCameraModule() : nullptr;
        if (!camera_module ||
            !camera_module->SetCameraPose(case_config.camera.position, case_config.camera.euler_angles, true))
        {
            out_error = "Failed to apply camera pose.";
            return false;
        }
    }

    Regression::LogicPackContext logic_context{};
    logic_context.directional_light_speed_radians = &m_directional_light_angular_speed_radians;
    logic_context.lighting = m_lighting.get();
    logic_context.ssao = m_ssao.get();
    logic_context.texture_debug_view = m_texture_debug_view.get();
    if (!Regression::ApplyLogicPack(case_config, logic_context, out_error))
    {
        return false;
    }

    return true;
}

bool DemoAppModelViewer::StartRegressionCase(const RendererInterface::RenderGraph::FrameStats& frame_stats)
{
    if (m_regression_case_index >= m_regression_suite.cases.size())
    {
        return false;
    }

    const auto& case_config = m_regression_suite.cases[m_regression_case_index];
    const std::string case_id = case_config.id;
    ResetRegressionPerfAccumulator();

    std::string apply_error{};
    if (!m_regression_baseline_snapshot)
    {
        apply_error = "Regression baseline state snapshot is missing.";
    }
    else if (!ApplyNonRenderStateSnapshot(m_regression_baseline_snapshot))
    {
        apply_error = "Failed to restore regression baseline state snapshot.";
    }
    if (!apply_error.empty())
    {
        RegressionCaseResult failed{};
        failed.id = case_id;
        failed.success = false;
        failed.error = apply_error;
        m_regression_case_results.push_back(std::move(failed));
        LOG_FORMAT_FLUSH("[Regression] Case '%s' failed to start: %s\n", case_id.c_str(), apply_error.c_str());
        return false;
    }

    if (!ApplyRegressionCaseConfig(case_config, apply_error))
    {
        RegressionCaseResult failed{};
        failed.id = case_id;
        failed.success = false;
        failed.error = apply_error;
        m_regression_case_results.push_back(std::move(failed));
        LOG_FORMAT_FLUSH("[Regression] Case '%s' failed to start: %s\n", case_id.c_str(), apply_error.c_str());
        return false;
    }

    m_regression_case_start_frame = frame_stats.frame_index;
    m_regression_case_last_elapsed_frames = 0;
    m_regression_case_active = true;

    LOG_FORMAT_FLUSH("[Regression] Case %u/%u start: %s\n",
                     static_cast<unsigned>(m_regression_case_index + 1u),
                     static_cast<unsigned>(m_regression_suite.cases.size()),
                     case_id.c_str());
    return true;
}

bool DemoAppModelViewer::CaptureWindowScreenshotPNG(const std::filesystem::path& file_path) const
{
    if (!m_window)
    {
        return false;
    }

    const HWND hwnd = m_window->GetHWND();
    if (!hwnd)
    {
        return false;
    }

    RECT client_rect{};
    if (!GetClientRect(hwnd, &client_rect))
    {
        return false;
    }

    const int width = client_rect.right - client_rect.left;
    const int height = client_rect.bottom - client_rect.top;
    if (width <= 0 || height <= 0)
    {
        return false;
    }

    HDC window_dc = GetDC(hwnd);
    if (!window_dc)
    {
        return false;
    }

    HDC memory_dc = CreateCompatibleDC(window_dc);
    if (!memory_dc)
    {
        ReleaseDC(hwnd, window_dc);
        return false;
    }

    BITMAPINFO bitmap_info{};
    bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap_info.bmiHeader.biWidth = width;
    bitmap_info.bmiHeader.biHeight = -height;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32;
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    void* bitmap_bits = nullptr;
    HBITMAP dib_bitmap = CreateDIBSection(window_dc, &bitmap_info, DIB_RGB_COLORS, &bitmap_bits, nullptr, 0);
    if (!dib_bitmap || !bitmap_bits)
    {
        if (dib_bitmap)
        {
            DeleteObject(dib_bitmap);
        }
        DeleteDC(memory_dc);
        ReleaseDC(hwnd, window_dc);
        return false;
    }

    HGDIOBJ old_bitmap = SelectObject(memory_dc, dib_bitmap);
    BOOL capture_success = PrintWindow(hwnd, memory_dc, PW_RENDERFULLCONTENT | PW_CLIENTONLY);
    if (capture_success != TRUE)
    {
        capture_success = PrintWindow(hwnd, memory_dc, PW_CLIENTONLY);
    }
    if (capture_success != TRUE)
    {
        capture_success = BitBlt(memory_dc, 0, 0, width, height, window_dc, 0, 0, SRCCOPY | CAPTUREBLT);
    }
    GdiFlush();

    std::vector<unsigned char> rgba_data(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    if (capture_success == TRUE)
    {
        const unsigned char* bgra_data = static_cast<const unsigned char*>(bitmap_bits);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const size_t pixel_index = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                const size_t source_index = pixel_index * 4u;
                rgba_data[source_index + 0u] = bgra_data[source_index + 2u];
                rgba_data[source_index + 1u] = bgra_data[source_index + 1u];
                rgba_data[source_index + 2u] = bgra_data[source_index + 0u];
                rgba_data[source_index + 3u] = bgra_data[source_index + 3u];
            }
        }
    }

    SelectObject(memory_dc, old_bitmap);
    DeleteObject(dib_bitmap);
    DeleteDC(memory_dc);
    ReleaseDC(hwnd, window_dc);

    if (capture_success != TRUE)
    {
        return false;
    }

    return glTFImageIOUtil::Instance().SaveAsPNG(file_path.string(), rgba_data.data(), width, height);
}

bool DemoAppModelViewer::FinalizeRegressionCase(const RendererInterface::RenderGraph::FrameStats& frame_stats)
{
    if (m_regression_case_index >= m_regression_suite.cases.size())
    {
        return false;
    }

    const auto& case_config = m_regression_suite.cases[m_regression_case_index];
    const std::string case_prefix = BuildRegressionCasePrefix(
        static_cast<unsigned>(m_regression_case_index + 1u),
        case_config.id);

    const std::filesystem::path case_dir = m_regression_output_root / "cases";
    const std::filesystem::path screenshot_path = case_dir / (case_prefix + ".png");
    const std::filesystem::path pass_csv_path = case_dir / (case_prefix + ".pass.csv");
    const std::filesystem::path perf_json_path = case_dir / (case_prefix + ".perf.json");

    RegressionCaseResult result{};
    result.id = case_config.id;
    result.success = true;

    if (!WriteRegressionPassCsv(frame_stats, pass_csv_path))
    {
        result.success = false;
        result.error += "failed_to_write_pass_csv;";
    }
    else
    {
        result.pass_csv_path = pass_csv_path.string();
    }

    if (!WriteRegressionPerfJson(case_config, frame_stats, perf_json_path))
    {
        result.success = false;
        result.error += "failed_to_write_perf_json;";
    }
    else
    {
        result.perf_json_path = perf_json_path.string();
    }

    if (case_config.capture.capture_screenshot)
    {
        if (!CaptureWindowScreenshotPNG(screenshot_path))
        {
            result.success = false;
            result.error += "failed_to_capture_screenshot;";
        }
        else
        {
            result.screenshot_path = screenshot_path.string();
        }
    }

    m_regression_case_results.push_back(result);

    LOG_FORMAT_FLUSH("[Regression] Case %u/%u done: %s (%s)\n",
                     static_cast<unsigned>(m_regression_case_index + 1u),
                     static_cast<unsigned>(m_regression_suite.cases.size()),
                     case_config.id.c_str(),
                     result.success ? "OK" : "FAILED");
    return result.success;
}

bool DemoAppModelViewer::FinalizeRegressionRun()
{
    if (m_regression_finished)
    {
        return true;
    }

    nlohmann::json summary{};
    summary["suite_name"] = m_regression_suite.suite_name;
    summary["suite_path"] = m_regression_suite.source_path.string();
    summary["output_root"] = m_regression_output_root.string();
    summary["render_device"] = ToString(m_render_device_type);
    summary["present_mode"] = ToString(
        m_resource_manager ? m_resource_manager->GetSwapchainPresentMode() : m_swapchain_present_mode_ui);
    summary["renderdoc_capture_enabled"] = false;
    summary["renderdoc_capture_available"] = false;
    summary["renderdoc_capture_required"] = false;
    summary["renderdoc_capture_forced"] = false;
    summary["case_count"] = m_regression_suite.cases.size();
    summary["result_count"] = m_regression_case_results.size();

    nlohmann::json case_results = nlohmann::json::array();
    unsigned failed_count = 0u;
    for (const auto& result : m_regression_case_results)
    {
        if (!result.success)
        {
            failed_count += 1u;
        }

        nlohmann::json case_item{};
        case_item["id"] = result.id;
        case_item["success"] = result.success;
        case_item["screenshot_path"] = result.screenshot_path;
        case_item["pass_csv_path"] = result.pass_csv_path;
        case_item["perf_json_path"] = result.perf_json_path;
        case_item["renderdoc_capture_success"] = false;
        case_item["renderdoc_capture_retained"] = false;
        case_item["renderdoc_capture_keep_on_success"] = true;
        case_item["renderdoc_capture_frame_index"] = 0;
        case_item["renderdoc_capture_path"] = "";
        case_item["renderdoc_capture_error"] = "";
        case_item["error"] = result.error;
        case_results.push_back(std::move(case_item));
    }

    summary["failed_count"] = failed_count;
    summary["renderdoc_capture_success_count"] = 0;
    summary["renderdoc_capture_retained_count"] = 0;
    summary["success"] =
        failed_count == 0u &&
        m_regression_case_results.size() == m_regression_suite.cases.size();
    summary["cases"] = std::move(case_results);

    const std::filesystem::path summary_path = m_regression_output_root / "suite_result.json";
    std::ofstream summary_stream(summary_path, std::ios::out | std::ios::trunc);
    if (!summary_stream.is_open())
    {
        LOG_FORMAT_FLUSH("[Regression] Failed to write suite summary: %s\n", summary_path.string().c_str());
        return false;
    }
    summary_stream << summary.dump(2) << "\n";
    summary_stream.close();

    m_regression_last_summary_path = summary_path.string();
    m_regression_finished = true;
    LOG_FORMAT_FLUSH("[Regression] Suite completed. Summary: %s\n", summary_path.string().c_str());
    return true;
}

void DemoAppModelViewer::TickRegressionAutomation()
{
    if (!m_regression_enabled || !m_render_graph || m_regression_finished)
    {
        return;
    }

    const auto& frame_stats = m_render_graph->GetLastFrameStats();

    if (!m_regression_case_active)
    {
        while (m_regression_case_index < m_regression_suite.cases.size())
        {
            if (StartRegressionCase(frame_stats))
            {
                break;
            }
            m_regression_case_index += 1u;
        }

        if (m_regression_case_index >= m_regression_suite.cases.size())
        {
            FinalizeRegressionRun();
            if (m_window)
            {
                m_window->RequestClose();
            }
            return;
        }
    }

    const auto& case_config = m_regression_suite.cases[m_regression_case_index];
    const unsigned long long elapsed_frames = frame_stats.frame_index >= m_regression_case_start_frame
        ? (frame_stats.frame_index - m_regression_case_start_frame)
        : 0ull;
    if (elapsed_frames == m_regression_case_last_elapsed_frames)
    {
        return;
    }
    m_regression_case_last_elapsed_frames = elapsed_frames;

    const unsigned long long capture_begin = static_cast<unsigned long long>(case_config.capture.warmup_frames) + 1ull;
    const unsigned long long capture_end = GetRegressionCaptureEndElapsedFrames(case_config);
    if (elapsed_frames >= capture_begin && elapsed_frames <= capture_end)
    {
        AccumulateRegressionPerf(frame_stats);
    }

    if (elapsed_frames >= capture_end)
    {
        FinalizeRegressionCase(frame_stats);
        m_regression_case_index += 1u;
        m_regression_case_active = false;

        if (m_regression_case_index >= m_regression_suite.cases.size())
        {
            FinalizeRegressionRun();
            if (m_window)
            {
                m_window->RequestClose();
            }
        }
    }
}

bool DemoAppModelViewer::ApplyModelViewerStateSnapshot(const ModelViewerStateSnapshot& snapshot)
{
    m_directional_light_info = snapshot.directional_light_info;
    m_directional_light_elapsed_seconds = snapshot.directional_light_elapsed_seconds;
    m_directional_light_angular_speed_radians = snapshot.directional_light_speed_radians;

    if (m_scene)
    {
        const auto camera_module = m_scene->GetCameraModule();
        if (camera_module)
        {
            if (snapshot.has_camera_pose)
            {
                camera_module->SetCameraPose(
                    snapshot.camera_position,
                    snapshot.camera_euler_angles,
                    true);
            }
            if (snapshot.camera_viewport_width > 0 && snapshot.camera_viewport_height > 0)
            {
                camera_module->SetViewportSize(
                    snapshot.camera_viewport_width,
                    snapshot.camera_viewport_height);
            }
        }
    }

    if (m_lighting)
    {
        m_lighting->UpdateLight(m_directional_light_index, m_directional_light_info);
        if (snapshot.has_lighting_state)
        {
            m_lighting->SetGlobalParams(snapshot.lighting_global_params);
        }
    }
    if (m_tone_map && snapshot.has_tone_map_state)
    {
        m_tone_map->SetGlobalParams(snapshot.tone_map_global_params);
    }
    if (m_ssao && snapshot.has_ssao_state)
    {
        m_ssao->SetGlobalParams(snapshot.ssao_global_params);
    }
    if (m_texture_debug_view && snapshot.has_texture_debug_state)
    {
        if (!m_texture_debug_view->SetDebugState(snapshot.texture_debug_state))
        {
            return false;
        }
    }

    return true;
}

bool DemoAppModelViewer::RebuildModelViewerBaseRuntimeObjects()
{
    if (!m_resource_manager)
    {
        return false;
    }

    m_modules.clear();
    m_systems.clear();
    m_scene.reset();
    m_ssao.reset();
    m_lighting.reset();
    m_tone_map.reset();
    m_texture_debug_view.reset();

    const unsigned render_width = (std::max)(1u, m_resource_manager->GetCurrentRenderWidth());
    const unsigned render_height = (std::max)(1u, m_resource_manager->GetCurrentRenderHeight());

    RendererCameraDesc camera_desc{};
    camera_desc.mode = CameraMode::Free;
    camera_desc.transform = glm::mat4(1.0f);
    camera_desc.fov_angle = 90.0f;
    camera_desc.projection_far = 1000.0f;
    camera_desc.projection_near = 0.001f;
    camera_desc.projection_width = static_cast<float>(render_width);
    camera_desc.projection_height = static_cast<float>(render_height);

    m_scene = std::make_shared<RendererSystemSceneRenderer>(
        *m_resource_manager,
        camera_desc,
        "glTFResources/Models/Sponza/glTF/Sponza.gltf");
    m_ssao = std::make_shared<RendererSystemSSAO>(m_scene);
    m_lighting = std::make_shared<RendererSystemLighting>(*m_resource_manager, m_scene, m_ssao);

    LightInfo directional_light_info{};
    directional_light_info.type = Directional;
    directional_light_info.intensity = {1.0f, 1.0f, 1.0f};
    directional_light_info.position = {0.0f, -0.707f, -0.707f};
    directional_light_info.radius = 100000.0f;
    m_directional_light_info = directional_light_info;
    m_directional_light_index = m_lighting->AddLight(m_directional_light_info);

    m_systems.push_back(m_scene);
    m_systems.push_back(m_ssao);
    m_systems.push_back(m_lighting);
    return true;
}

bool DemoAppModelViewer::FinalizeModelViewerRuntimeObjects(
    const std::shared_ptr<RendererSystemFrostedGlass>& tone_map_frosted_input)
{
    m_tone_map = std::make_shared<RendererSystemToneMap>(tone_map_frosted_input, m_lighting);
    m_texture_debug_view = std::make_shared<RendererSystemTextureDebugView>(m_tone_map);

    const auto scene = m_scene;
    const auto ssao = m_ssao;
    const auto lighting = m_lighting;
    const auto tone_map = m_tone_map;
    const auto frosted = tone_map_frosted_input;
    const auto register_source = [this](RendererSystemTextureDebugView::DebugSourceDesc source_desc) -> bool
    {
        return m_texture_debug_view && m_texture_debug_view->RegisterSource(std::move(source_desc));
    };

    RETURN_IF_FALSE(register_source({
        .id = "final.tonemapped",
        .display_name = "Final Tonemapped",
        .visualization = RendererSystemTextureDebugView::VisualizationMode::Color,
        .default_scale = 1.0f,
        .default_bias = 0.0f,
        .default_apply_tonemap = false,
        .resolver = [tone_map]()
        {
            return RendererSystemTextureDebugView::SourceResolution{
                .render_target = tone_map ? tone_map->GetOutput() : NULL_HANDLE,
                .dependency_node = tone_map ? tone_map->GetOutputNode() : NULL_HANDLE
            };
        }
    }));
    RETURN_IF_FALSE(register_source({
        .id = "scene.color",
        .display_name = "Scene Color",
        .visualization = RendererSystemTextureDebugView::VisualizationMode::Color,
        .default_scale = 1.0f,
        .default_bias = 0.0f,
        .default_apply_tonemap = false,
        .resolver = [scene]()
        {
            const auto outputs = scene ? scene->GetOutputs() : RendererSystemSceneRenderer::BasePassOutputs{};
            return RendererSystemTextureDebugView::SourceResolution{
                .render_target = outputs.color,
                .dependency_node = outputs.node
            };
        }
    }));
    RETURN_IF_FALSE(register_source({
        .id = "scene.normal",
        .display_name = "Scene Normal",
        .visualization = RendererSystemTextureDebugView::VisualizationMode::Color,
        .default_scale = 1.0f,
        .default_bias = 0.0f,
        .default_apply_tonemap = false,
        .resolver = [scene]()
        {
            const auto outputs = scene ? scene->GetOutputs() : RendererSystemSceneRenderer::BasePassOutputs{};
            return RendererSystemTextureDebugView::SourceResolution{
                .render_target = outputs.normal,
                .dependency_node = outputs.node
            };
        }
    }));
    RETURN_IF_FALSE(register_source({
        .id = "scene.velocity",
        .display_name = "Scene Velocity",
        .visualization = RendererSystemTextureDebugView::VisualizationMode::Velocity,
        .default_scale = 32.0f,
        .default_bias = 0.0f,
        .default_apply_tonemap = false,
        .resolver = [scene]()
        {
            const auto outputs = scene ? scene->GetOutputs() : RendererSystemSceneRenderer::BasePassOutputs{};
            return RendererSystemTextureDebugView::SourceResolution{
                .render_target = outputs.velocity,
                .dependency_node = outputs.node
            };
        }
    }));
    RETURN_IF_FALSE(register_source({
        .id = "scene.depth",
        .display_name = "Scene Depth",
        .visualization = RendererSystemTextureDebugView::VisualizationMode::Scalar,
        .default_scale = -1.0f,
        .default_bias = 1.0f,
        .default_apply_tonemap = false,
        .resolver = [scene]()
        {
            const auto outputs = scene ? scene->GetOutputs() : RendererSystemSceneRenderer::BasePassOutputs{};
            return RendererSystemTextureDebugView::SourceResolution{
                .render_target = outputs.depth,
                .dependency_node = outputs.node
            };
        }
    }));
    RETURN_IF_FALSE(register_source({
        .id = "ssao.raw",
        .display_name = "SSAO Raw",
        .visualization = RendererSystemTextureDebugView::VisualizationMode::Scalar,
        .default_scale = 1.0f,
        .default_bias = 0.0f,
        .default_apply_tonemap = false,
        .resolver = [ssao]()
        {
            const auto outputs = ssao ? ssao->GetOutputs() : RendererSystemSSAO::SSAOOutputs{};
            return RendererSystemTextureDebugView::SourceResolution{
                .render_target = outputs.raw_output,
                .dependency_node = outputs.raw_node
            };
        }
    }));
    RETURN_IF_FALSE(register_source({
        .id = "ssao.final",
        .display_name = "SSAO Final",
        .visualization = RendererSystemTextureDebugView::VisualizationMode::Scalar,
        .default_scale = 1.0f,
        .default_bias = 0.0f,
        .default_apply_tonemap = false,
        .resolver = [ssao]()
        {
            return RendererSystemTextureDebugView::SourceResolution{
                .render_target = ssao ? ssao->GetOutput() : NULL_HANDLE,
                .dependency_node = ssao ? ssao->GetOutputNode() : NULL_HANDLE
            };
        }
    }));
    RETURN_IF_FALSE(register_source({
        .id = "lighting.output",
        .display_name = "Lighting HDR",
        .visualization = RendererSystemTextureDebugView::VisualizationMode::Color,
        .default_scale = 1.0f,
        .default_bias = 0.0f,
        .default_apply_tonemap = true,
        .resolver = [lighting]()
        {
            const auto outputs = lighting ? lighting->GetOutputs() : RendererSystemLighting::LightingOutputs{};
            return RendererSystemTextureDebugView::SourceResolution{
                .render_target = outputs.output,
                .dependency_node = outputs.node
            };
        }
    }));
    if (frosted)
    {
        RETURN_IF_FALSE(register_source({
            .id = "frosted.output",
            .display_name = "Frosted HDR",
            .visualization = RendererSystemTextureDebugView::VisualizationMode::Color,
            .default_scale = 1.0f,
            .default_bias = 0.0f,
            .default_apply_tonemap = true,
            .resolver = [frosted]()
            {
                return RendererSystemTextureDebugView::SourceResolution{
                    .render_target = frosted ? frosted->GetOutput() : NULL_HANDLE,
                    .dependency_node = frosted ? frosted->GetOutputNode() : NULL_HANDLE
                };
            }
        }));
    }

    m_systems.push_back(m_tone_map);
    m_systems.push_back(m_texture_debug_view);
    return true;
}

bool DemoAppModelViewer::RebuildRenderRuntimeObjects()
{
    if (!RebuildModelViewerBaseRuntimeObjects())
    {
        return false;
    }

    return FinalizeModelViewerRuntimeObjects(nullptr);
}

bool DemoAppModelViewer::InitInternal(const std::vector<std::string>& arguments)
{
    if (!RebuildRenderRuntimeObjects())
    {
        return false;
    }

    return ConfigureRegressionRunFromArguments(arguments);
}

void DemoAppModelViewer::TickFrameInternal(unsigned long long time_interval)
{
    const bool freeze_directional_light =
        m_regression_enabled && m_regression_suite.freeze_directional_light;
    UpdateModelViewerFrame(time_interval, !m_regression_enabled, freeze_directional_light);

    if (m_regression_enabled)
    {
        TickRegressionAutomation();
    }

    if (m_window)
    {
        m_window->GetInputDevice().TickFrame(time_interval);
    }
}

void DemoAppModelViewer::DrawModelViewerBaseDebugUI()
{
    ImGui::SliderFloat("Directional Light Speed", &m_directional_light_angular_speed_radians, 0.0f, 2.0f, "%.3f rad/s");
    if (m_regression_enabled)
    {
        ImGui::Separator();
        ImGui::Text("Regression Suite: %s", m_regression_suite.suite_name.c_str());
        ImGui::Text("Regression Output: %s", m_regression_output_root.string().c_str());
        ImGui::Text("Regression Progress: %u / %u",
                    static_cast<unsigned>((std::min)(
                        m_regression_case_index + (m_regression_case_active ? 1u : 0u),
                        m_regression_suite.cases.size())),
                    static_cast<unsigned>(m_regression_suite.cases.size()));
        ImGui::Text("Regression State: %s", m_regression_finished ? "Finished" : "Running");
        if (!m_regression_last_summary_path.empty())
        {
            ImGui::Text("Regression Summary: %s", m_regression_last_summary_path.c_str());
        }
    }
}

void DemoAppModelViewer::DrawDebugUIInternal()
{
    if (ImGui::CollapsingHeader("Demo"))
    {
        DrawModelViewerBaseDebugUI();
    }
}

std::shared_ptr<DemoBase::NonRenderStateSnapshot> DemoAppModelViewer::CaptureNonRenderStateSnapshot() const
{
    return CaptureModelViewerStateSnapshot();
}

bool DemoAppModelViewer::ApplyNonRenderStateSnapshot(const std::shared_ptr<NonRenderStateSnapshot>& snapshot)
{
    if (!snapshot)
    {
        return true;
    }

    const auto restored_state = std::dynamic_pointer_cast<ModelViewerStateSnapshot>(snapshot);
    if (!restored_state)
    {
        return false;
    }

    return ApplyModelViewerStateSnapshot(*restored_state);
}

bool DemoAppModelViewer::SerializeNonRenderStateSnapshotToJson(
    const std::shared_ptr<NonRenderStateSnapshot>& snapshot,
    nlohmann::json& out_snapshot_json) const
{
    const auto model_viewer_snapshot = std::dynamic_pointer_cast<ModelViewerStateSnapshot>(snapshot);
    if (!model_viewer_snapshot)
    {
        return false;
    }

    out_snapshot_json = {
        {"camera", {
            {"has_pose", model_viewer_snapshot->has_camera_pose},
            {"position", ToJson(model_viewer_snapshot->camera_position)},
            {"euler_angles", ToJson(model_viewer_snapshot->camera_euler_angles)},
            {"viewport_width", model_viewer_snapshot->camera_viewport_width},
            {"viewport_height", model_viewer_snapshot->camera_viewport_height}
        }},
        {"directional_light", {
            {"position", ToJson(model_viewer_snapshot->directional_light_info.position)},
            {"radius", model_viewer_snapshot->directional_light_info.radius},
            {"intensity", ToJson(model_viewer_snapshot->directional_light_info.intensity)},
            {"type", static_cast<unsigned>(model_viewer_snapshot->directional_light_info.type)},
            {"elapsed_seconds", model_viewer_snapshot->directional_light_elapsed_seconds},
            {"speed_radians", model_viewer_snapshot->directional_light_speed_radians}
        }}
    };

    if (model_viewer_snapshot->has_lighting_state)
    {
        out_snapshot_json["lighting"] = {
            {"sky_zenith_color", ToJson(model_viewer_snapshot->lighting_global_params.sky_zenith_color)},
            {"sky_horizon_color", ToJson(model_viewer_snapshot->lighting_global_params.sky_horizon_color)},
            {"ground_color", ToJson(model_viewer_snapshot->lighting_global_params.ground_color)},
            {"environment_control", ToJson(model_viewer_snapshot->lighting_global_params.environment_control)},
            {"environment_texture_params", ToJson(model_viewer_snapshot->lighting_global_params.environment_texture_params)},
            {"environment_prefilter_roughness", ToJson(model_viewer_snapshot->lighting_global_params.environment_prefilter_roughness)}
        };
    }
    if (model_viewer_snapshot->has_tone_map_state)
    {
        out_snapshot_json["tonemap"] = {
            {"exposure", model_viewer_snapshot->tone_map_global_params.exposure},
            {"gamma", model_viewer_snapshot->tone_map_global_params.gamma},
            {"tone_map_mode", model_viewer_snapshot->tone_map_global_params.tone_map_mode}
        };
    }
    if (model_viewer_snapshot->has_ssao_state)
    {
        out_snapshot_json["ssao"] = {
            {"radius_world", model_viewer_snapshot->ssao_global_params.radius_world},
            {"intensity", model_viewer_snapshot->ssao_global_params.intensity},
            {"power", model_viewer_snapshot->ssao_global_params.power},
            {"bias", model_viewer_snapshot->ssao_global_params.bias},
            {"thickness", model_viewer_snapshot->ssao_global_params.thickness},
            {"sample_distribution_power", model_viewer_snapshot->ssao_global_params.sample_distribution_power},
            {"blur_depth_reject", model_viewer_snapshot->ssao_global_params.blur_depth_reject},
            {"blur_normal_reject", model_viewer_snapshot->ssao_global_params.blur_normal_reject},
            {"sample_count", model_viewer_snapshot->ssao_global_params.sample_count},
            {"blur_radius", model_viewer_snapshot->ssao_global_params.blur_radius},
            {"enabled", model_viewer_snapshot->ssao_global_params.enabled != 0u},
            {"debug_output_mode", model_viewer_snapshot->ssao_global_params.debug_output_mode}
        };
    }
    if (model_viewer_snapshot->has_texture_debug_state)
    {
        out_snapshot_json["texture_debug"] = {
            {"source_id", model_viewer_snapshot->texture_debug_state.source_id},
            {"scale", model_viewer_snapshot->texture_debug_state.scale},
            {"bias", model_viewer_snapshot->texture_debug_state.bias},
            {"apply_tonemap", model_viewer_snapshot->texture_debug_state.apply_tonemap}
        };
    }

    return true;
}

std::shared_ptr<DemoBase::NonRenderStateSnapshot> DemoAppModelViewer::DeserializeNonRenderStateSnapshotFromJson(
    const nlohmann::json& snapshot_json,
    std::string& out_error) const
{
    if (!snapshot_json.is_object())
    {
        out_error = "snapshot must be an object.";
        return nullptr;
    }
    if (!snapshot_json.contains("camera") || !snapshot_json.at("camera").is_object())
    {
        out_error = "snapshot.camera must be an object.";
        return nullptr;
    }
    if (!snapshot_json.contains("directional_light") || !snapshot_json.at("directional_light").is_object())
    {
        out_error = "snapshot.directional_light must be an object.";
        return nullptr;
    }

    auto snapshot = std::make_shared<ModelViewerStateSnapshot>();

    const auto& camera_json = snapshot_json.at("camera");
    if (!camera_json.contains("has_pose") || !camera_json.at("has_pose").is_boolean())
    {
        out_error = "snapshot.camera.has_pose must be a boolean.";
        return nullptr;
    }
    snapshot->has_camera_pose = camera_json.at("has_pose").get<bool>();

    if (!camera_json.contains("position") ||
        !ReadVec3(camera_json.at("position"), snapshot->camera_position, out_error))
    {
        if (out_error.empty())
        {
            out_error = "snapshot.camera.position is invalid.";
        }
        return nullptr;
    }
    if (!camera_json.contains("euler_angles") ||
        !ReadVec3(camera_json.at("euler_angles"), snapshot->camera_euler_angles, out_error))
    {
        if (out_error.empty())
        {
            out_error = "snapshot.camera.euler_angles is invalid.";
        }
        return nullptr;
    }
    if (!camera_json.contains("viewport_width") || !camera_json.at("viewport_width").is_number_unsigned())
    {
        out_error = "snapshot.camera.viewport_width must be an unsigned number.";
        return nullptr;
    }
    if (!camera_json.contains("viewport_height") || !camera_json.at("viewport_height").is_number_unsigned())
    {
        out_error = "snapshot.camera.viewport_height must be an unsigned number.";
        return nullptr;
    }
    snapshot->camera_viewport_width = camera_json.at("viewport_width").get<unsigned>();
    snapshot->camera_viewport_height = camera_json.at("viewport_height").get<unsigned>();

    const auto& light_json = snapshot_json.at("directional_light");
    if (!light_json.contains("position") ||
        !ReadVec3(light_json.at("position"), snapshot->directional_light_info.position, out_error))
    {
        if (out_error.empty())
        {
            out_error = "snapshot.directional_light.position is invalid.";
        }
        return nullptr;
    }
    if (!light_json.contains("intensity") ||
        !ReadVec3(light_json.at("intensity"), snapshot->directional_light_info.intensity, out_error))
    {
        if (out_error.empty())
        {
            out_error = "snapshot.directional_light.intensity is invalid.";
        }
        return nullptr;
    }
    if (!light_json.contains("radius") || !light_json.at("radius").is_number())
    {
        out_error = "snapshot.directional_light.radius must be a number.";
        return nullptr;
    }
    if (!light_json.contains("type") || !light_json.at("type").is_number_unsigned())
    {
        out_error = "snapshot.directional_light.type must be an unsigned number.";
        return nullptr;
    }
    if (!light_json.contains("elapsed_seconds") || !light_json.at("elapsed_seconds").is_number())
    {
        out_error = "snapshot.directional_light.elapsed_seconds must be a number.";
        return nullptr;
    }
    if (!light_json.contains("speed_radians") || !light_json.at("speed_radians").is_number())
    {
        out_error = "snapshot.directional_light.speed_radians must be a number.";
        return nullptr;
    }

    snapshot->directional_light_info.radius = light_json.at("radius").get<float>();
    snapshot->directional_light_info.type = static_cast<LightType>(light_json.at("type").get<unsigned>());
    snapshot->directional_light_elapsed_seconds = light_json.at("elapsed_seconds").get<float>();
    snapshot->directional_light_speed_radians = light_json.at("speed_radians").get<float>();

    if (snapshot_json.contains("lighting"))
    {
        const auto& lighting_json = snapshot_json.at("lighting");
        if (!lighting_json.is_object())
        {
            out_error = "snapshot.lighting must be an object.";
            return nullptr;
        }
        if (!lighting_json.contains("sky_zenith_color") ||
            !ReadVec4(lighting_json.at("sky_zenith_color"), snapshot->lighting_global_params.sky_zenith_color, out_error))
        {
            if (out_error.empty())
            {
                out_error = "snapshot.lighting.sky_zenith_color is invalid.";
            }
            return nullptr;
        }
        if (!lighting_json.contains("sky_horizon_color") ||
            !ReadVec4(lighting_json.at("sky_horizon_color"), snapshot->lighting_global_params.sky_horizon_color, out_error))
        {
            if (out_error.empty())
            {
                out_error = "snapshot.lighting.sky_horizon_color is invalid.";
            }
            return nullptr;
        }
        if (!lighting_json.contains("ground_color") ||
            !ReadVec4(lighting_json.at("ground_color"), snapshot->lighting_global_params.ground_color, out_error))
        {
            if (out_error.empty())
            {
                out_error = "snapshot.lighting.ground_color is invalid.";
            }
            return nullptr;
        }
        if (!lighting_json.contains("environment_control") ||
            !ReadVec4(lighting_json.at("environment_control"), snapshot->lighting_global_params.environment_control, out_error))
        {
            if (out_error.empty())
            {
                out_error = "snapshot.lighting.environment_control is invalid.";
            }
            return nullptr;
        }
        if (!lighting_json.contains("environment_texture_params") ||
            !ReadVec4(lighting_json.at("environment_texture_params"), snapshot->lighting_global_params.environment_texture_params, out_error))
        {
            if (out_error.empty())
            {
                out_error = "snapshot.lighting.environment_texture_params is invalid.";
            }
            return nullptr;
        }
        if (!lighting_json.contains("environment_prefilter_roughness") ||
            !ReadVec4(lighting_json.at("environment_prefilter_roughness"), snapshot->lighting_global_params.environment_prefilter_roughness, out_error))
        {
            if (out_error.empty())
            {
                out_error = "snapshot.lighting.environment_prefilter_roughness is invalid.";
            }
            return nullptr;
        }
        snapshot->has_lighting_state = true;
    }
    if (snapshot_json.contains("tonemap"))
    {
        const auto& tonemap_json = snapshot_json.at("tonemap");
        if (!tonemap_json.is_object())
        {
            out_error = "snapshot.tonemap must be an object.";
            return nullptr;
        }
        if (!tonemap_json.contains("exposure") || !tonemap_json.at("exposure").is_number())
        {
            out_error = "snapshot.tonemap.exposure must be a number.";
            return nullptr;
        }
        if (!tonemap_json.contains("gamma") || !tonemap_json.at("gamma").is_number())
        {
            out_error = "snapshot.tonemap.gamma must be a number.";
            return nullptr;
        }
        if (!tonemap_json.contains("tone_map_mode") || !tonemap_json.at("tone_map_mode").is_number_unsigned())
        {
            out_error = "snapshot.tonemap.tone_map_mode must be an unsigned number.";
            return nullptr;
        }

        snapshot->tone_map_global_params.exposure = tonemap_json.at("exposure").get<float>();
        snapshot->tone_map_global_params.gamma = tonemap_json.at("gamma").get<float>();
        snapshot->tone_map_global_params.tone_map_mode = tonemap_json.at("tone_map_mode").get<unsigned>();
        snapshot->has_tone_map_state = true;
    }
    if (snapshot_json.contains("ssao"))
    {
        const auto& ssao_json = snapshot_json.at("ssao");
        if (!ssao_json.is_object())
        {
            out_error = "snapshot.ssao must be an object.";
            return nullptr;
        }
        if (!ssao_json.contains("radius_world") || !ssao_json.at("radius_world").is_number())
        {
            out_error = "snapshot.ssao.radius_world must be a number.";
            return nullptr;
        }
        if (!ssao_json.contains("intensity") || !ssao_json.at("intensity").is_number())
        {
            out_error = "snapshot.ssao.intensity must be a number.";
            return nullptr;
        }
        if (!ssao_json.contains("power") || !ssao_json.at("power").is_number())
        {
            out_error = "snapshot.ssao.power must be a number.";
            return nullptr;
        }
        if (!ssao_json.contains("bias") || !ssao_json.at("bias").is_number())
        {
            out_error = "snapshot.ssao.bias must be a number.";
            return nullptr;
        }
        if (!ssao_json.contains("thickness") || !ssao_json.at("thickness").is_number())
        {
            out_error = "snapshot.ssao.thickness must be a number.";
            return nullptr;
        }
        if (!ssao_json.contains("sample_distribution_power") || !ssao_json.at("sample_distribution_power").is_number())
        {
            out_error = "snapshot.ssao.sample_distribution_power must be a number.";
            return nullptr;
        }
        if (!ssao_json.contains("blur_depth_reject") || !ssao_json.at("blur_depth_reject").is_number())
        {
            out_error = "snapshot.ssao.blur_depth_reject must be a number.";
            return nullptr;
        }
        if (!ssao_json.contains("blur_normal_reject") || !ssao_json.at("blur_normal_reject").is_number())
        {
            out_error = "snapshot.ssao.blur_normal_reject must be a number.";
            return nullptr;
        }
        if (!ssao_json.contains("sample_count") || !ssao_json.at("sample_count").is_number_unsigned())
        {
            out_error = "snapshot.ssao.sample_count must be an unsigned number.";
            return nullptr;
        }
        if (!ssao_json.contains("blur_radius") || !ssao_json.at("blur_radius").is_number_unsigned())
        {
            out_error = "snapshot.ssao.blur_radius must be an unsigned number.";
            return nullptr;
        }
        if (!ssao_json.contains("enabled") || !ssao_json.at("enabled").is_boolean())
        {
            out_error = "snapshot.ssao.enabled must be a boolean.";
            return nullptr;
        }
        if (ssao_json.contains("debug_output_mode") && !ssao_json.at("debug_output_mode").is_number_unsigned())
        {
            out_error = "snapshot.ssao.debug_output_mode must be an unsigned number.";
            return nullptr;
        }

        snapshot->ssao_global_params.radius_world = ssao_json.at("radius_world").get<float>();
        snapshot->ssao_global_params.intensity = ssao_json.at("intensity").get<float>();
        snapshot->ssao_global_params.power = ssao_json.at("power").get<float>();
        snapshot->ssao_global_params.bias = ssao_json.at("bias").get<float>();
        snapshot->ssao_global_params.thickness = ssao_json.at("thickness").get<float>();
        snapshot->ssao_global_params.sample_distribution_power =
            ssao_json.at("sample_distribution_power").get<float>();
        snapshot->ssao_global_params.blur_depth_reject = ssao_json.at("blur_depth_reject").get<float>();
        snapshot->ssao_global_params.blur_normal_reject = ssao_json.at("blur_normal_reject").get<float>();
        snapshot->ssao_global_params.sample_count = ssao_json.at("sample_count").get<unsigned>();
        snapshot->ssao_global_params.blur_radius = ssao_json.at("blur_radius").get<unsigned>();
        snapshot->ssao_global_params.enabled = ssao_json.at("enabled").get<bool>() ? 1u : 0u;
        snapshot->ssao_global_params.debug_output_mode = ssao_json.contains("debug_output_mode")
            ? ssao_json.at("debug_output_mode").get<unsigned>()
            : 0u;
        snapshot->has_ssao_state = true;
    }
    if (snapshot_json.contains("texture_debug"))
    {
        const auto& texture_debug_json = snapshot_json.at("texture_debug");
        if (!texture_debug_json.is_object())
        {
            out_error = "snapshot.texture_debug must be an object.";
            return nullptr;
        }
        if (!texture_debug_json.contains("source_id") || !texture_debug_json.at("source_id").is_string())
        {
            out_error = "snapshot.texture_debug.source_id must be a string.";
            return nullptr;
        }
        if (!texture_debug_json.contains("scale") || !texture_debug_json.at("scale").is_number())
        {
            out_error = "snapshot.texture_debug.scale must be a number.";
            return nullptr;
        }
        if (!texture_debug_json.contains("bias") || !texture_debug_json.at("bias").is_number())
        {
            out_error = "snapshot.texture_debug.bias must be a number.";
            return nullptr;
        }
        if (!texture_debug_json.contains("apply_tonemap") || !texture_debug_json.at("apply_tonemap").is_boolean())
        {
            out_error = "snapshot.texture_debug.apply_tonemap must be a boolean.";
            return nullptr;
        }

        snapshot->texture_debug_state.source_id = texture_debug_json.at("source_id").get<std::string>();
        snapshot->texture_debug_state.scale = texture_debug_json.at("scale").get<float>();
        snapshot->texture_debug_state.bias = texture_debug_json.at("bias").get<float>();
        snapshot->texture_debug_state.apply_tonemap = texture_debug_json.at("apply_tonemap").get<bool>();
        snapshot->has_texture_debug_state = true;
    }

    return snapshot;
}
