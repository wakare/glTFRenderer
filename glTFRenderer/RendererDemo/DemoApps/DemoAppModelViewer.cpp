#include "DemoAppModelViewer.h"

#include "RenderWindow/RendererInputDevice.h"
#include <glm/glm/gtx/norm.hpp>
#include <cmath>
#include <imgui/imgui.h>

#include "RendererCommon.h"
#include "RendererModule/RendererModuleCamera.h"
#include "RendererModule/RendererModuleSceneMesh.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

void DemoAppModelViewer::TickFrameInternal(unsigned long long time_interval)
{
    auto& input_device = m_window->GetInputDevice();
    m_scene->UpdateInputDeviceInfo(input_device, time_interval);

    const float delta_seconds = static_cast<float>(time_interval) / 1000.0f;
    m_directional_light_elapsed_seconds += delta_seconds;
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

    if (m_enable_panel_input_state_machine &&
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
        else if (left_mouse_pressed)
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

    UpdateFrostedPanelPrepassFeeds(m_directional_light_elapsed_seconds);

    if (m_auto_export_b7_snapshot && !m_auto_export_b7_snapshot_done && m_render_graph)
    {
        const auto& frame_stats = m_render_graph->GetLastFrameStats();
        if (frame_stats.frame_index >= static_cast<unsigned long long>((std::max)(m_b7_snapshot_warmup_frames, 1)))
        {
            m_auto_export_b7_snapshot_done = ExportB7AcceptanceSnapshot();
        }
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

    return true;
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
        ImGui::TextUnformatted("Panel state mapping: move mouse=Hover, LMB=Move, RMB=Grab, Ctrl+LMB=Scale.");
    }
}
