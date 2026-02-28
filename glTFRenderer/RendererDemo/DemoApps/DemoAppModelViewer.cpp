#include "DemoAppModelViewer.h"

#include "RenderWindow/RendererInputDevice.h"
#include "Regression/RegressionLogicPack.h"
#include "SceneFileLoader/glTFImageIOUtil.h"
#include <glm/glm/gtx/norm.hpp>
#include <cmath>
#include <imgui/imgui.h>

#include "RendererCommon.h"
#include "RendererModule/RendererModuleCamera.h"
#include "RendererModule/RendererModuleSceneMesh.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <nlohmann_json/single_include/nlohmann/json.hpp>
#include <sstream>
#include <windows.h>

namespace
{
    constexpr double PANEL_MOVE_CURSOR_DEADZONE_SQ = 0.04; // ~0.2 px

    std::string SanitizeFileName(std::string value)
    {
        for (char& ch : value)
        {
            if ((ch >= 'a' && ch <= 'z') ||
                (ch >= 'A' && ch <= 'Z') ||
                (ch >= '0' && ch <= '9') ||
                ch == '-' || ch == '_')
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
}

void DemoAppModelViewer::TickFrameInternal(unsigned long long time_interval)
{
    auto& input_device = m_window->GetInputDevice();
    if (!m_regression_enabled)
    {
        m_scene->UpdateInputDeviceInfo(input_device, time_interval);
    }

    const float delta_seconds = static_cast<float>(time_interval) / 1000.0f;
    const bool freeze_directional_light =
        m_regression_enabled && m_regression_suite.freeze_directional_light;
    if (!freeze_directional_light)
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
    m_directional_light_info.position = rotated_direction;
    m_lighting->UpdateLight(m_directional_light_index, m_directional_light_info);

    if (!m_regression_enabled &&
        m_enable_panel_input_state_machine &&
        m_frosted_glass &&
        m_frosted_glass->ContainsPanel(0))
    {
        auto panel_state = RendererSystemFrostedGlass::PanelInteractionState::Idle;
        const bool left_mouse_pressed = input_device.IsMouseButtonPressed(InputDeviceButtonType::MOUSE_BUTTON_LEFT);
        const bool right_mouse_pressed = input_device.IsMouseButtonPressed(InputDeviceButtonType::MOUSE_BUTTON_RIGHT);
        const bool ctrl_pressed = input_device.IsKeyPressed(InputDeviceKeyType::KEY_LEFT_CONTROL) ||
                                  input_device.IsKeyPressed(InputDeviceKeyType::KEY_RIGHT_CONTROL);
        const auto cursor_offset = input_device.GetCursorOffset();
        const double cursor_offset_len_sq = cursor_offset.X * cursor_offset.X + cursor_offset.Y * cursor_offset.Y;
        if (left_mouse_pressed && ctrl_pressed)
        {
            panel_state = RendererSystemFrostedGlass::PanelInteractionState::Scale;
        }
        else if (left_mouse_pressed && cursor_offset_len_sq > PANEL_MOVE_CURSOR_DEADZONE_SQ)
        {
            panel_state = RendererSystemFrostedGlass::PanelInteractionState::Move;
        }
        else if (right_mouse_pressed)
        {
            panel_state = RendererSystemFrostedGlass::PanelInteractionState::Grab;
        }
        else if (cursor_offset_len_sq > 1e-4)
        {
            panel_state = RendererSystemFrostedGlass::PanelInteractionState::Hover;
        }

        m_frosted_glass->SetPanelInteractionState(0, panel_state);
    }

    const bool freeze_prepass_animation =
        m_regression_enabled && m_regression_suite.disable_prepass_animation;
    UpdateFrostedPanelPrepassFeeds(freeze_prepass_animation ? 0.0f : m_directional_light_elapsed_seconds);

    if (m_auto_export_b7_snapshot && !m_auto_export_b7_snapshot_done && m_render_graph)
    {
        const auto& frame_stats = m_render_graph->GetLastFrameStats();
        if (frame_stats.frame_index >= static_cast<unsigned long long>((std::max)(m_b7_snapshot_warmup_frames, 1)))
        {
            m_auto_export_b7_snapshot_done = ExportB7AcceptanceSnapshot();
        }
    }

    if (m_regression_enabled)
    {
        TickRegressionAutomation();
    }
    
    input_device.TickFrame(time_interval);
}

bool DemoAppModelViewer::InitInternal(const std::vector<std::string>& arguments)
{
    RendererCameraDesc camera_desc{};
    camera_desc.mode = CameraMode::Free;
    camera_desc.transform = glm::mat4(1.0f);
    camera_desc.fov_angle = 90.0f;
    camera_desc.projection_far = 1000.0f;
    camera_desc.projection_near = 0.001f;
    camera_desc.projection_width = static_cast<float>(m_width);
    camera_desc.projection_height = static_cast<float>(m_height);

    m_scene = std::make_shared<RendererSystemSceneRenderer>(*m_resource_manager, camera_desc, "glTFResources/Models/Sponza/glTF/Sponza.gltf");
    m_lighting = std::make_shared<RendererSystemLighting>(*m_resource_manager, m_scene);
    m_frosted_glass = std::make_shared<RendererSystemFrostedGlass>(m_scene, m_lighting);
    m_frosted_panel_producer = std::make_shared<RendererSystemFrostedPanelProducer>(m_frosted_glass);
    m_tone_map = std::make_shared<RendererSystemToneMap>(m_frosted_glass, m_scene);
    {
        RendererSystemFrostedGlass::FrostedGlassPanelDesc panel_desc{};
        panel_desc.center_uv = {0.5f, 0.52f};
        panel_desc.half_size_uv = {0.30f, 0.20f};
        panel_desc.corner_radius = 0.03f;
        panel_desc.blur_sigma = 8.5f;
        panel_desc.blur_strength = 0.92f;
        panel_desc.rim_intensity = 0.03f;
        panel_desc.tint_color = {0.94f, 0.97f, 1.0f};
        panel_desc.depth_weight_scale = 100.0f;
        panel_desc.shape_type = RendererSystemFrostedGlass::PanelShapeType::RoundedRect;
        panel_desc.edge_softness = 1.25f;
        panel_desc.thickness = 0.014f;
        panel_desc.refraction_strength = 0.90f;
        panel_desc.fresnel_intensity = 0.02f;
        panel_desc.fresnel_power = 6.0f;
        panel_desc.layer_order = 0.0f;
        m_frosted_glass->AddPanel(panel_desc);

        RendererSystemFrostedGlass::FrostedGlassPanelDesc overlay_style_desc = panel_desc;
        overlay_style_desc.corner_radius = 0.02f;
        overlay_style_desc.blur_sigma = 11.0f;
        overlay_style_desc.blur_strength = 0.96f;
        overlay_style_desc.rim_intensity = 0.04f;
        overlay_style_desc.tint_color = {0.92f, 0.97f, 1.0f};
        overlay_style_desc.shape_type = RendererSystemFrostedGlass::PanelShapeType::ShapeMask;
        overlay_style_desc.custom_shape_index = 2.0f;
        overlay_style_desc.depth_policy = RendererSystemFrostedGlass::PanelDepthPolicy::Overlay;
        m_frosted_panel_producer->SetOverlayPanelDesc(overlay_style_desc);

        RendererSystemFrostedGlass::FrostedGlassPanelDesc world_style_desc = panel_desc;
        world_style_desc.world_space_mode = 1u;
        world_style_desc.depth_policy = RendererSystemFrostedGlass::PanelDepthPolicy::SceneOcclusion;
        world_style_desc.center_uv = {0.5f, 0.5f};
        world_style_desc.half_size_uv = {0.22f, 0.14f};
        world_style_desc.corner_radius = 0.02f;
        world_style_desc.blur_sigma = 10.5f;
        world_style_desc.blur_strength = 0.95f;
        world_style_desc.layer_order = -0.5f;
        world_style_desc.shape_type = RendererSystemFrostedGlass::PanelShapeType::RoundedRect;
        world_style_desc.custom_shape_index = 0.0f;
        m_frosted_panel_producer->SetWorldPanelDesc(world_style_desc);

        m_world_prepass_panels = {
            {
                glm::fvec3(0.0f, 1.20f, -0.90f),
                glm::fvec3(0.56f, 0.0f, 0.0f),
                glm::fvec3(0.0f, 0.34f, 0.0f),
                -0.5f,
                RendererSystemFrostedGlass::PanelDepthPolicy::SceneOcclusion,
                RendererSystemFrostedGlass::PanelShapeType::RoundedRect,
                0.0f,
                0.02f,
                1.0f
            }
        };
        m_overlay_prepass_panels = {
            {
                glm::fvec2(0.62f, 0.48f),
                glm::fvec2(0.16f, 0.12f),
                1.0f,
                RendererSystemFrostedGlass::PanelShapeType::ShapeMask,
                2.0f,
                0.02f,
                1.0f
            }
        };
        UpdateFrostedPanelPrepassFeeds(0.0f);
    }
    
    // Add test light
    LightInfo directional_light_info{};
    directional_light_info.type = Directional;
    directional_light_info.intensity = {1.0f, 1.0f, 1.0f};
    directional_light_info.position = {0.0f, -0.707f, -0.707f};
    directional_light_info.radius = 100000.0f;
    m_directional_light_info = directional_light_info;
    m_directional_light_index = m_lighting->AddLight(m_directional_light_info);

    m_systems.push_back(m_scene);
    m_systems.push_back(m_lighting);
    m_systems.push_back(m_frosted_panel_producer);
    m_systems.push_back(m_frosted_glass);
    m_systems.push_back(m_tone_map);
    
    // After registration all passes, compile graph and prepare for execution
    m_render_graph->CompileRenderPassAndExecute();

    const bool configured = ConfigureRegressionRunFromArguments(arguments);
    RefreshImportableRegressionCaseList();
    return configured;
}

void DemoAppModelViewer::UpdateFrostedPanelPrepassFeeds(float timeline_seconds)
{
    if (!m_frosted_panel_producer)
    {
        return;
    }

    if (!m_enable_frosted_prepass_feeds)
    {
        m_frosted_panel_producer->ClearPrepassItems();
        return;
    }

    if (!m_world_prepass_panels.empty())
    {
        auto& world_panel = m_world_prepass_panels[0];
        world_panel.world_center.x = 0.10f * std::sin(timeline_seconds * 0.35f);
        world_panel.world_center.y = 1.20f + 0.02f * std::sin(timeline_seconds * 0.20f);
    }
    if (!m_overlay_prepass_panels.empty())
    {
        auto& overlay_panel = m_overlay_prepass_panels[0];
        overlay_panel.center_uv.x = 0.62f + 0.02f * std::sin(timeline_seconds * 0.28f);
    }

    m_frosted_panel_producer->SetWorldPanelPrepassItems(m_world_prepass_panels);
    m_frosted_panel_producer->SetOverlayPanelPrepassItems(m_overlay_prepass_panels);
}

bool DemoAppModelViewer::ExportB7AcceptanceSnapshot()
{
    if (!m_render_graph)
    {
        return false;
    }

    const auto& frame_stats = m_render_graph->GetLastFrameStats();
    if (frame_stats.pass_stats.empty())
    {
        return false;
    }

    std::error_code fs_error;
    std::filesystem::create_directories("build_logs", fs_error);
    if (fs_error)
    {
        return false;
    }

    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm{};
    localtime_s(&local_tm, &now_time);
    std::ostringstream stamp_stream;
    stamp_stream << std::put_time(&local_tm, "%Y%m%d_%H%M%S");
    const std::string stamp = stamp_stream.str();

    const std::filesystem::path summary_path = std::filesystem::path("build_logs") / ("b7_acceptance_snapshot_" + stamp + ".md");
    const std::filesystem::path csv_path = std::filesystem::path("build_logs") / ("b7_acceptance_passstats_" + stamp + ".csv");

    std::ofstream csv_stream(csv_path, std::ios::out | std::ios::trunc);
    if (!csv_stream.is_open())
    {
        return false;
    }
    csv_stream << "frame_index,group,pass,executed,skipped_validation,cpu_ms,gpu_valid,gpu_ms\n";
    for (const auto& pass_stat : frame_stats.pass_stats)
    {
        csv_stream << frame_stats.frame_index << ","
                   << "\"" << pass_stat.group_name << "\"" << ","
                   << "\"" << pass_stat.pass_name << "\"" << ","
                   << (pass_stat.executed ? 1 : 0) << ","
                   << (pass_stat.skipped_due_to_validation ? 1 : 0) << ","
                   << std::fixed << std::setprecision(4) << pass_stat.cpu_time_ms << ","
                   << (pass_stat.gpu_time_valid ? 1 : 0) << ","
                   << std::fixed << std::setprecision(4) << pass_stat.gpu_time_ms << "\n";
    }
    csv_stream.close();

    float frosted_cpu_total = 0.0f;
    float frosted_gpu_total = 0.0f;
    bool frosted_gpu_valid = false;
    unsigned frosted_pass_count = 0;
    for (const auto& pass_stat : frame_stats.pass_stats)
    {
        if (pass_stat.group_name != "Frosted Glass")
        {
            continue;
        }
        ++frosted_pass_count;
        frosted_cpu_total += pass_stat.cpu_time_ms;
        if (pass_stat.gpu_time_valid)
        {
            frosted_gpu_valid = true;
            frosted_gpu_total += pass_stat.gpu_time_ms;
        }
    }

    std::ofstream summary_stream(summary_path, std::ios::out | std::ios::trunc);
    if (!summary_stream.is_open())
    {
        return false;
    }
    summary_stream << "# B7 Acceptance Snapshot\n\n";
    summary_stream << "- Frame Index: " << frame_stats.frame_index << "\n";
    summary_stream << "- CPU Total (ms): " << std::fixed << std::setprecision(3) << frame_stats.cpu_total_ms << "\n";
    summary_stream << "- GPU Total (ms): "
                   << (frame_stats.gpu_time_valid ? std::to_string(frame_stats.gpu_total_ms) : std::string("N/A")) << "\n";
    summary_stream << "- Executed Pass Count: " << frame_stats.executed_pass_count << "\n";
    summary_stream << "- Skipped Pass Count: " << frame_stats.skipped_pass_count << "\n";
    summary_stream << "- Skipped Validation Pass Count: " << frame_stats.skipped_validation_pass_count << "\n";
    summary_stream << "- Frosted Pass Count: " << frosted_pass_count << "\n";
    summary_stream << "- Frosted CPU Total (ms): " << std::fixed << std::setprecision(3) << frosted_cpu_total << "\n";
    summary_stream << "- Frosted GPU Total (ms): "
                   << (frosted_gpu_valid ? std::to_string(frosted_gpu_total) : std::string("N/A")) << "\n";
    summary_stream << "- Pass CSV: `" << csv_path.string() << "`\n\n";
    summary_stream << "## Frosted Pass Entries\n\n";
    summary_stream << "| Pass | Executed | CPU ms | GPU ms |\n";
    summary_stream << "|---|---|---:|---:|\n";
    for (const auto& pass_stat : frame_stats.pass_stats)
    {
        if (pass_stat.group_name != "Frosted Glass")
        {
            continue;
        }
        summary_stream << "| " << pass_stat.pass_name
                       << " | " << (pass_stat.executed ? "Y" : "N")
                       << " | " << std::fixed << std::setprecision(3) << pass_stat.cpu_time_ms
                       << " | " << (pass_stat.gpu_time_valid ? std::to_string(pass_stat.gpu_time_ms) : std::string("N/A"))
                       << " |\n";
    }
    summary_stream.close();

    m_last_b7_snapshot_summary_path = summary_path.string();
    m_last_b7_snapshot_csv_path = csv_path.string();
    return true;
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
        return true;
    }
    if (suite_path_arg.empty())
    {
        LOG_FORMAT_FLUSH("[Regression] Missing -regression-suite=<path>.\n");
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
    if (m_regression_suite.disable_panel_input_state_machine)
    {
        m_enable_panel_input_state_machine = false;
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

    float frosted_cpu_ms = 0.0f;
    float frosted_gpu_ms = 0.0f;
    bool frosted_gpu_valid = false;
    for (const auto& pass_stat : frame_stats.pass_stats)
    {
        if (pass_stat.group_name != "Frosted Glass")
        {
            continue;
        }
        frosted_cpu_ms += pass_stat.cpu_time_ms;
        if (pass_stat.gpu_time_valid)
        {
            frosted_gpu_ms += pass_stat.gpu_time_ms;
            frosted_gpu_valid = true;
        }
    }

    m_regression_perf_accumulator.frosted_cpu_sum_ms += frosted_cpu_ms;
    if (frosted_gpu_valid)
    {
        m_regression_perf_accumulator.frosted_gpu_valid_count += 1u;
        m_regression_perf_accumulator.frosted_gpu_sum_ms += frosted_gpu_ms;
    }
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
    logic_context.frosted_glass = m_frosted_glass.get();
    logic_context.enable_panel_input_state_machine = &m_enable_panel_input_state_machine;
    logic_context.enable_frosted_prepass_feeds = &m_enable_frosted_prepass_feeds;
    logic_context.directional_light_speed_radians = &m_directional_light_angular_speed_radians;
    if (!Regression::ApplyLogicPack(case_config, logic_context, out_error))
    {
        return false;
    }

    if (m_frosted_glass)
    {
        m_frosted_glass->ForceResetTemporalHistory();
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

    if (m_regression_suite.disable_panel_input_state_machine)
    {
        m_enable_panel_input_state_machine = false;
    }
    if (m_regression_suite.disable_prepass_animation)
    {
        m_enable_frosted_prepass_feeds = true;
    }

    std::string apply_error{};
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
    const BOOL blit_success = BitBlt(memory_dc, 0, 0, width, height, window_dc, 0, 0, SRCCOPY | CAPTUREBLT);
    GdiFlush();

    std::vector<unsigned char> rgba_data(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    if (blit_success == TRUE)
    {
        const unsigned char* bgra_data = static_cast<const unsigned char*>(bitmap_bits);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const size_t pixel_index = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                const size_t source_index = pixel_index * 4u;
                const size_t dest_index = source_index;
                rgba_data[dest_index + 0u] = bgra_data[source_index + 2u];
                rgba_data[dest_index + 1u] = bgra_data[source_index + 1u];
                rgba_data[dest_index + 2u] = bgra_data[source_index + 0u];
                rgba_data[dest_index + 3u] = bgra_data[source_index + 3u];
            }
        }
    }

    SelectObject(memory_dc, old_bitmap);
    DeleteObject(dib_bitmap);
    DeleteDC(memory_dc);
    ReleaseDC(hwnd, window_dc);

    if (blit_success != TRUE)
    {
        return false;
    }

    return glTFImageIOUtil::Instance().SaveAsPNG(file_path.string(), rgba_data.data(), width, height);
}

bool DemoAppModelViewer::WriteRegressionPassCsv(const RendererInterface::RenderGraph::FrameStats& frame_stats,
                                                const std::filesystem::path& file_path) const
{
    std::ofstream csv_stream(file_path, std::ios::out | std::ios::trunc);
    if (!csv_stream.is_open())
    {
        return false;
    }

    csv_stream << "frame_index,group,pass,executed,skipped_validation,cpu_ms,gpu_valid,gpu_ms\n";
    for (const auto& pass_stat : frame_stats.pass_stats)
    {
        csv_stream << frame_stats.frame_index << ","
                   << "\"" << pass_stat.group_name << "\"" << ","
                   << "\"" << pass_stat.pass_name << "\"" << ","
                   << (pass_stat.executed ? 1 : 0) << ","
                   << (pass_stat.skipped_due_to_validation ? 1 : 0) << ","
                   << std::fixed << std::setprecision(4) << pass_stat.cpu_time_ms << ","
                   << (pass_stat.gpu_time_valid ? 1 : 0) << ","
                   << std::fixed << std::setprecision(4) << pass_stat.gpu_time_ms << "\n";
    }

    return true;
}

bool DemoAppModelViewer::WriteRegressionPerfJson(const RendererInterface::RenderGraph::FrameStats& frame_stats,
                                                 const std::filesystem::path& file_path) const
{
    const auto safe_avg = [](double sum, unsigned count) -> double
    {
        return count > 0u ? (sum / static_cast<double>(count)) : 0.0;
    };

    nlohmann::json summary{};
    summary["capture_frame_index"] = frame_stats.frame_index;
    summary["sample_count"] = m_regression_perf_accumulator.sample_count;
    summary["cpu_total_avg_ms"] = safe_avg(m_regression_perf_accumulator.cpu_total_sum_ms,
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
    summary["frosted_cpu_avg_ms"] = safe_avg(
        m_regression_perf_accumulator.frosted_cpu_sum_ms,
        m_regression_perf_accumulator.sample_count);
    if (m_regression_perf_accumulator.frosted_gpu_valid_count > 0u)
    {
        summary["frosted_gpu_avg_ms"] = safe_avg(
            m_regression_perf_accumulator.frosted_gpu_sum_ms,
            m_regression_perf_accumulator.frosted_gpu_valid_count);
    }
    else
    {
        summary["frosted_gpu_avg_ms"] = nullptr;
    }

    std::ofstream json_stream(file_path, std::ios::out | std::ios::trunc);
    if (!json_stream.is_open())
    {
        return false;
    }
    json_stream << summary.dump(2) << "\n";
    return true;
}

bool DemoAppModelViewer::FinalizeRegressionCase(const RendererInterface::RenderGraph::FrameStats& frame_stats)
{
    if (m_regression_case_index >= m_regression_suite.cases.size())
    {
        return false;
    }

    const auto& case_config = m_regression_suite.cases[m_regression_case_index];
    const std::string case_name = SanitizeFileName(case_config.id);
    std::ostringstream case_prefix_stream;
    case_prefix_stream << std::setfill('0') << std::setw(3) << (m_regression_case_index + 1u) << "_" << case_name;
    const std::string case_prefix = case_prefix_stream.str();

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

    if (!WriteRegressionPerfJson(frame_stats, perf_json_path))
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
        case_item["error"] = result.error;
        case_results.push_back(std::move(case_item));
    }
    summary["failed_count"] = failed_count;
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

bool DemoAppModelViewer::ExportCurrentRegressionCaseTemplate()
{
    if (!m_scene)
    {
        return false;
    }

    const auto camera_module = m_scene->GetCameraModule();
    if (!camera_module)
    {
        return false;
    }

    glm::fvec3 camera_position{0.0f};
    glm::fvec3 camera_euler{0.0f};
    if (!camera_module->GetCameraPose(camera_position, camera_euler))
    {
        return false;
    }

    std::string case_id = SanitizeFileName(m_regression_export_case_id);
    if (case_id.empty())
    {
        case_id = "case_export";
    }

    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm{};
    localtime_s(&local_tm, &now_time);
    std::ostringstream stamp_stream;
    stamp_stream << std::put_time(&local_tm, "%Y%m%d_%H%M%S");
    const std::string stamp = stamp_stream.str();

    std::filesystem::path output_dir = std::filesystem::path("build_logs") / "regression_case_exports";
    std::error_code fs_error;
    std::filesystem::create_directories(output_dir, fs_error);
    if (fs_error)
    {
        return false;
    }

    nlohmann::json root{};
    root["suite_name"] = case_id + "_suite";
    root["global"] = {
        {"disable_debug_ui", true},
        {"freeze_directional_light", true},
        {"disable_panel_input_state_machine", true},
        {"disable_prepass_animation", true},
        {"default_warmup_frames", 180},
        {"default_capture_frames", 16}
    };

    nlohmann::json case_json{};
    case_json["id"] = case_id;
    case_json["camera"] = {
        {"position", {camera_position.x, camera_position.y, camera_position.z}},
        {"euler_angles", {camera_euler.x, camera_euler.y, camera_euler.z}}
    };

    if (m_frosted_glass)
    {
        std::string blur_source_mode = "shared_mip";
        switch (m_frosted_glass->GetBlurSourceMode())
        {
        case RendererSystemFrostedGlass::BlurSourceMode::LegacyPyramid:
            blur_source_mode = "legacy_pyramid";
            break;
        case RendererSystemFrostedGlass::BlurSourceMode::SharedMip:
            blur_source_mode = "shared_mip";
            break;
        case RendererSystemFrostedGlass::BlurSourceMode::SharedDual:
            blur_source_mode = "shared_dual";
            break;
        default:
            break;
        }

        case_json["logic_pack"] = "frosted_glass";
        case_json["logic_args"] = {
            {"blur_source_mode", blur_source_mode},
            {"full_fog_mode", m_frosted_glass->IsFullFogModeEnabled()},
            {"reset_temporal_history", true},
            {"enable_panel_input_state_machine", m_enable_panel_input_state_machine},
            {"enable_frosted_prepass_feeds", m_enable_frosted_prepass_feeds},
            {"directional_light_speed_radians", m_directional_light_angular_speed_radians}
        };
    }
    else
    {
        case_json["logic_pack"] = "none";
        case_json["logic_args"] = nlohmann::json::object();
    }

    root["cases"] = nlohmann::json::array();
    root["cases"].push_back(case_json);

    const std::filesystem::path output_path = output_dir / (case_id + "_" + stamp + ".json");
    std::ofstream output_stream(output_path, std::ios::out | std::ios::trunc);
    if (!output_stream.is_open())
    {
        return false;
    }

    output_stream << root.dump(2) << "\n";
    output_stream.close();

    m_last_regression_case_export_path = output_path.string();
    RefreshImportableRegressionCaseList();
    LOG_FORMAT_FLUSH("[Regression] Exported current case json: %s\n", m_last_regression_case_export_path.c_str());
    return true;
}

void DemoAppModelViewer::RefreshImportableRegressionCaseList()
{
    m_regression_import_case_entries.clear();
    m_regression_import_selected_index = -1;

    std::filesystem::path import_dir = std::filesystem::path(m_regression_import_directory);
    if (import_dir.empty())
    {
        m_last_regression_case_import_status = "Import directory is empty.";
        return;
    }

    std::error_code fs_error;
    if (!std::filesystem::exists(import_dir, fs_error) || fs_error)
    {
        m_last_regression_case_import_status = "Import directory does not exist: " + import_dir.string();
        return;
    }
    if (!std::filesystem::is_directory(import_dir, fs_error) || fs_error)
    {
        m_last_regression_case_import_status = "Import path is not a directory: " + import_dir.string();
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(
             import_dir, std::filesystem::directory_options::skip_permission_denied, fs_error))
    {
        if (fs_error)
        {
            break;
        }

        std::error_code entry_error;
        if (!entry.is_regular_file(entry_error) || entry_error)
        {
            continue;
        }

        std::string extension = entry.path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value)
        {
            return static_cast<char>(std::tolower(value));
        });
        if (extension != ".json")
        {
            continue;
        }

        RegressionImportCaseEntry import_entry{};
        import_entry.file_name = entry.path().filename().string();
        import_entry.full_path = std::filesystem::absolute(entry.path(), entry_error);
        if (entry_error)
        {
            import_entry.full_path = entry.path();
        }
        m_regression_import_case_entries.push_back(std::move(import_entry));
    }

    std::sort(m_regression_import_case_entries.begin(), m_regression_import_case_entries.end(),
        [](const RegressionImportCaseEntry& lhs, const RegressionImportCaseEntry& rhs)
    {
        return lhs.file_name > rhs.file_name;
    });

    if (!m_regression_import_case_entries.empty())
    {
        m_regression_import_selected_index = 0;
        m_last_regression_case_import_status =
            "Found " + std::to_string(m_regression_import_case_entries.size()) + " importable case JSON file(s).";
    }
    else
    {
        m_last_regression_case_import_status = "No importable case JSON files found.";
    }
}

bool DemoAppModelViewer::ImportRegressionCaseFromJson(const std::filesystem::path& suite_path)
{
    Regression::SuiteConfig imported_suite{};
    std::string load_error{};
    if (!Regression::LoadSuiteConfig(suite_path, imported_suite, load_error))
    {
        m_last_regression_case_import_status = "Import failed: " + load_error;
        LOG_FORMAT_FLUSH("[Regression] Import failed for '%s': %s\n", suite_path.string().c_str(), load_error.c_str());
        return false;
    }
    if (imported_suite.cases.empty())
    {
        m_last_regression_case_import_status = "Import failed: suite has no cases.";
        LOG_FORMAT_FLUSH("[Regression] Import failed for '%s': suite has no cases.\n", suite_path.string().c_str());
        return false;
    }

    if (imported_suite.disable_panel_input_state_machine)
    {
        m_enable_panel_input_state_machine = false;
    }

    std::string apply_error{};
    if (!ApplyRegressionCaseConfig(imported_suite.cases.front(), apply_error))
    {
        m_last_regression_case_import_status = "Import failed to apply case: " + apply_error;
        LOG_FORMAT_FLUSH("[Regression] Import apply failed for '%s': %s\n", suite_path.string().c_str(), apply_error.c_str());
        return false;
    }

    const float prepass_timeline = imported_suite.disable_prepass_animation
        ? 0.0f
        : m_directional_light_elapsed_seconds;
    UpdateFrostedPanelPrepassFeeds(prepass_timeline);

    std::error_code path_error;
    const std::filesystem::path absolute_path = std::filesystem::absolute(suite_path, path_error);
    m_last_regression_case_import_path = (path_error ? suite_path : absolute_path).string();

    std::ostringstream status_stream;
    status_stream << "Imported case '" << imported_suite.cases.front().id
                  << "' from " << m_last_regression_case_import_path;
    if (imported_suite.cases.size() > 1u)
    {
        status_stream << " (applied first case of " << imported_suite.cases.size() << ")";
    }
    m_last_regression_case_import_status = status_stream.str();
    LOG_FORMAT_FLUSH("[Regression] %s\n", m_last_regression_case_import_status.c_str());
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

    const unsigned capture_begin = case_config.capture.warmup_frames + 1u;
    const unsigned capture_end = case_config.capture.warmup_frames + case_config.capture.capture_frames;
    if (case_config.capture.capture_perf &&
        elapsed_frames >= capture_begin &&
        elapsed_frames <= capture_end)
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

void DemoAppModelViewer::DrawDebugUIInternal()
{
    if (ImGui::CollapsingHeader("Demo", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SliderFloat("Directional Light Speed", &m_directional_light_angular_speed_radians, 0.0f, 2.0f, "%.3f rad/s");
        ImGui::Checkbox("Enable Panel Input State Machine", &m_enable_panel_input_state_machine);
        if (ImGui::Checkbox("Enable Frosted Prepass Feeds", &m_enable_frosted_prepass_feeds))
        {
            UpdateFrostedPanelPrepassFeeds(m_directional_light_elapsed_seconds);
        }
        if (ImGui::Button("Export B7 Acceptance Snapshot"))
        {
            ExportB7AcceptanceSnapshot();
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Auto Export B7 Snapshot", &m_auto_export_b7_snapshot))
        {
            m_auto_export_b7_snapshot_done = false;
        }
        if (ImGui::SliderInt("B7 Snapshot Warmup Frames", &m_b7_snapshot_warmup_frames, 30, 1200))
        {
            m_b7_snapshot_warmup_frames = (std::max)(m_b7_snapshot_warmup_frames, 1);
        }
        ImGui::Text("B7 Snapshot Auto Export: %s", m_auto_export_b7_snapshot_done ? "Done" : "Pending");
        if (!m_last_b7_snapshot_summary_path.empty())
        {
            ImGui::Text("B7 Snapshot Summary: %s", m_last_b7_snapshot_summary_path.c_str());
            ImGui::Text("B7 Snapshot CSV: %s", m_last_b7_snapshot_csv_path.c_str());
        }
        if (m_frosted_glass)
        {
            ImGui::Text("Effective Frosted Panel Count: %u", m_frosted_glass->GetEffectivePanelCount());
        }
        ImGui::Text("Prepass Feed Panels: world=%u overlay=%u",
                    static_cast<unsigned>(m_world_prepass_panels.size()),
                    static_cast<unsigned>(m_overlay_prepass_panels.size()));
        if (m_regression_enabled)
        {
            ImGui::Separator();
            ImGui::Text("Regression Suite: %s", m_regression_suite.suite_name.c_str());
            ImGui::Text("Regression Output: %s", m_regression_output_root.string().c_str());
            ImGui::Text("Regression Progress: %u / %u",
                static_cast<unsigned>((std::min)(m_regression_case_index + (m_regression_case_active ? 1u : 0u),
                    m_regression_suite.cases.size())),
                static_cast<unsigned>(m_regression_suite.cases.size()));
            ImGui::Text("Regression State: %s", m_regression_finished ? "Finished" : "Running");
            if (!m_regression_last_summary_path.empty())
            {
                ImGui::Text("Regression Summary: %s", m_regression_last_summary_path.c_str());
            }
        }
        ImGui::Separator();
        ImGui::InputText("Export Case ID", m_regression_export_case_id, IM_ARRAYSIZE(m_regression_export_case_id));
        if (ImGui::Button("Export Current Regression Case JSON"))
        {
            ExportCurrentRegressionCaseTemplate();
        }
        if (!m_last_regression_case_export_path.empty())
        {
            ImGui::Text("Last Exported Case JSON: %s", m_last_regression_case_export_path.c_str());
        }
        ImGui::Separator();
        ImGui::InputText("Import Case Dir", m_regression_import_directory, IM_ARRAYSIZE(m_regression_import_directory));
        ImGui::SameLine();
        if (ImGui::Button("Refresh Importable Case JSONs"))
        {
            RefreshImportableRegressionCaseList();
        }
        if (ImGui::BeginListBox("Importable Case JSONs", ImVec2(0.0f, 120.0f)))
        {
            for (int index = 0; index < static_cast<int>(m_regression_import_case_entries.size()); ++index)
            {
                const auto& entry = m_regression_import_case_entries[static_cast<size_t>(index)];
                const bool selected = index == m_regression_import_selected_index;
                if (ImGui::Selectable(entry.file_name.c_str(), selected))
                {
                    m_regression_import_selected_index = index;
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndListBox();
        }
        if (ImGui::Button("Import Selected Case and Re-render"))
        {
            if (m_regression_import_selected_index >= 0 &&
                m_regression_import_selected_index < static_cast<int>(m_regression_import_case_entries.size()))
            {
                const auto& entry =
                    m_regression_import_case_entries[static_cast<size_t>(m_regression_import_selected_index)];
                ImportRegressionCaseFromJson(entry.full_path);
            }
            else
            {
                m_last_regression_case_import_status = "No importable case selected.";
            }
        }
        if (!m_last_regression_case_import_path.empty())
        {
            ImGui::Text("Last Imported Case JSON: %s", m_last_regression_case_import_path.c_str());
        }
        if (!m_last_regression_case_import_status.empty())
        {
            ImGui::TextWrapped("Import Status: %s", m_last_regression_case_import_status.c_str());
        }
        ImGui::TextUnformatted("Panel state mapping: move mouse=Hover, LMB+drag=Move, RMB=Grab, Ctrl+LMB=Scale.");
    }
}
