#include "DemoAppModelViewerFrostedGlass.h"

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

    unsigned long long GetRegressionCaptureEndElapsedFrames(const Regression::CaseConfig& case_config)
    {
        return static_cast<unsigned long long>(case_config.capture.warmup_frames) +
               static_cast<unsigned long long>(case_config.capture.capture_frames);
    }

    bool ShouldCaptureRegressionRenderDoc(
        const Regression::CaseConfig& case_config,
        bool force_renderdoc_capture)
    {
        return force_renderdoc_capture || case_config.capture.capture_renderdoc;
    }

    unsigned long long GetRegressionRenderDocTargetElapsedFrames(
        const Regression::CaseConfig& case_config,
        bool force_renderdoc_capture)
    {
        const unsigned long long capture_end = GetRegressionCaptureEndElapsedFrames(case_config);
        if (!ShouldCaptureRegressionRenderDoc(case_config, force_renderdoc_capture))
        {
            return capture_end;
        }

        return capture_end + static_cast<unsigned long long>(case_config.capture.renderdoc_capture_frame_offset);
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

    nlohmann::json ToJson(const glm::fvec2& value)
    {
        return nlohmann::json::array({value.x, value.y});
    }

    nlohmann::json ToJson(const glm::fvec3& value)
    {
        return nlohmann::json::array({value.x, value.y, value.z});
    }

    bool ReadVec2(const nlohmann::json& value, glm::fvec2& out_value, std::string& out_error)
    {
        if (!value.is_array() || value.size() != 2u)
        {
            out_error = "expected a 2-element array.";
            return false;
        }
        if (!value.at(0).is_number() || !value.at(1).is_number())
        {
            out_error = "vector elements must be numbers.";
            return false;
        }
        out_value = glm::fvec2(value.at(0).get<float>(), value.at(1).get<float>());
        return true;
    }

    bool ReadVec3(const nlohmann::json& value, glm::fvec3& out_value, std::string& out_error)
    {
        if (!value.is_array() || value.size() != 3u)
        {
            out_error = "expected a 3-element array.";
            return false;
        }
        if (!value.at(0).is_number() || !value.at(1).is_number() || !value.at(2).is_number())
        {
            out_error = "vector elements must be numbers.";
            return false;
        }
        out_value = glm::fvec3(value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>());
        return true;
    }
}

void DemoAppModelViewerFrostedGlass::TickFrameInternal(unsigned long long time_interval)
{
    const bool freeze_directional_light =
        m_regression_enabled && m_regression_suite.freeze_directional_light;
    UpdateModelViewerFrame(time_interval, !m_regression_enabled, freeze_directional_light);

    if (!m_window)
    {
        return;
    }

    auto& input_device = m_window->GetInputDevice();
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

    if (m_regression_enabled)
    {
        TickRegressionAutomation();
    }

    input_device.TickFrame(time_interval);
}

std::shared_ptr<DemoBase::NonRenderStateSnapshot> DemoAppModelViewerFrostedGlass::CaptureNonRenderStateSnapshot() const
{
    auto snapshot = std::make_shared<FrostedGlassStateSnapshot>();
    if (const auto base_snapshot = CaptureModelViewerStateSnapshot())
    {
        static_cast<ModelViewerStateSnapshot&>(*snapshot) = *base_snapshot;
    }

    snapshot->enable_panel_input_state_machine = m_enable_panel_input_state_machine;
    snapshot->enable_frosted_prepass_feeds = m_enable_frosted_prepass_feeds;
    snapshot->blur_source_mode = m_frosted_glass
        ? static_cast<unsigned>(m_frosted_glass->GetBlurSourceMode())
        : 0u;
    snapshot->full_fog_mode = m_frosted_glass ? m_frosted_glass->IsFullFogModeEnabled() : false;
    snapshot->world_prepass_panels = m_world_prepass_panels;
    snapshot->overlay_prepass_panels = m_overlay_prepass_panels;
    return snapshot;
}

bool DemoAppModelViewerFrostedGlass::ApplyNonRenderStateSnapshot(const std::shared_ptr<NonRenderStateSnapshot>& snapshot)
{
    if (!snapshot)
    {
        return true;
    }

    const auto restored_state = std::dynamic_pointer_cast<FrostedGlassStateSnapshot>(snapshot);
    if (!restored_state)
    {
        return false;
    }

    if (!ApplyModelViewerStateSnapshot(*restored_state))
    {
        return false;
    }

    m_enable_panel_input_state_machine = restored_state->enable_panel_input_state_machine;
    m_enable_frosted_prepass_feeds = restored_state->enable_frosted_prepass_feeds;
    m_world_prepass_panels = restored_state->world_prepass_panels;
    m_overlay_prepass_panels = restored_state->overlay_prepass_panels;
    if (m_frosted_glass)
    {
        m_frosted_glass->SetBlurSourceMode(
            static_cast<RendererSystemFrostedGlass::BlurSourceMode>(restored_state->blur_source_mode));
        m_frosted_glass->SetFullFogMode(restored_state->full_fog_mode);
    }

    if (m_frosted_panel_producer)
    {
        m_frosted_panel_producer->SetWorldPanelPrepassItems(m_world_prepass_panels);
        m_frosted_panel_producer->SetOverlayPanelPrepassItems(m_overlay_prepass_panels);
    }
    UpdateFrostedPanelPrepassFeeds(m_directional_light_elapsed_seconds);

    if (m_frosted_glass)
    {
        m_frosted_glass->ForceResetTemporalHistory();
    }
    return true;
}

bool DemoAppModelViewerFrostedGlass::SerializeNonRenderStateSnapshotToJson(
    const std::shared_ptr<NonRenderStateSnapshot>& snapshot,
    nlohmann::json& out_snapshot_json) const
{
    const auto frosted_snapshot = std::dynamic_pointer_cast<FrostedGlassStateSnapshot>(snapshot);
    if (!frosted_snapshot)
    {
        return false;
    }

    nlohmann::json base_json{};
    if (!DemoAppModelViewer::SerializeNonRenderStateSnapshotToJson(frosted_snapshot, base_json))
    {
        return false;
    }

    nlohmann::json world_panels = nlohmann::json::array();
    for (const auto& panel : frosted_snapshot->world_prepass_panels)
    {
        world_panels.push_back({
            {"world_center", ToJson(panel.world_center)},
            {"world_axis_u", ToJson(panel.world_axis_u)},
            {"world_axis_v", ToJson(panel.world_axis_v)},
            {"layer_order", panel.layer_order},
            {"depth_policy", static_cast<unsigned>(panel.depth_policy)},
            {"shape_type", static_cast<unsigned>(panel.shape_type)},
            {"custom_shape_index", panel.custom_shape_index},
            {"corner_radius", panel.corner_radius},
            {"panel_alpha", panel.panel_alpha}
        });
    }

    nlohmann::json overlay_panels = nlohmann::json::array();
    for (const auto& panel : frosted_snapshot->overlay_prepass_panels)
    {
        overlay_panels.push_back({
            {"center_uv", ToJson(panel.center_uv)},
            {"half_size_uv", ToJson(panel.half_size_uv)},
            {"layer_order", panel.layer_order},
            {"shape_type", static_cast<unsigned>(panel.shape_type)},
            {"custom_shape_index", panel.custom_shape_index},
            {"corner_radius", panel.corner_radius},
            {"panel_alpha", panel.panel_alpha}
        });
    }

    out_snapshot_json = {
        {"base", std::move(base_json)},
        {"frosted", {
            {"enable_panel_input_state_machine", frosted_snapshot->enable_panel_input_state_machine},
            {"enable_frosted_prepass_feeds", frosted_snapshot->enable_frosted_prepass_feeds},
            {"blur_source_mode", frosted_snapshot->blur_source_mode},
            {"full_fog_mode", frosted_snapshot->full_fog_mode},
            {"world_prepass_panels", std::move(world_panels)},
            {"overlay_prepass_panels", std::move(overlay_panels)}
        }}
    };
    return true;
}

std::shared_ptr<DemoBase::NonRenderStateSnapshot> DemoAppModelViewerFrostedGlass::DeserializeNonRenderStateSnapshotFromJson(
    const nlohmann::json& snapshot_json,
    std::string& out_error) const
{
    if (!snapshot_json.is_object())
    {
        out_error = "snapshot must be an object.";
        return nullptr;
    }
    if (!snapshot_json.contains("base") || !snapshot_json.at("base").is_object())
    {
        out_error = "snapshot.base must be an object.";
        return nullptr;
    }
    if (!snapshot_json.contains("frosted") || !snapshot_json.at("frosted").is_object())
    {
        out_error = "snapshot.frosted must be an object.";
        return nullptr;
    }

    const auto base_snapshot = std::dynamic_pointer_cast<ModelViewerStateSnapshot>(
        DemoAppModelViewer::DeserializeNonRenderStateSnapshotFromJson(snapshot_json.at("base"), out_error));
    if (!base_snapshot)
    {
        if (out_error.empty())
        {
            out_error = "failed to deserialize model viewer base snapshot.";
        }
        return nullptr;
    }

    const auto& frosted_json = snapshot_json.at("frosted");
    if (!frosted_json.contains("enable_panel_input_state_machine") ||
        !frosted_json.at("enable_panel_input_state_machine").is_boolean())
    {
        out_error = "snapshot.frosted.enable_panel_input_state_machine must be a boolean.";
        return nullptr;
    }
    if (!frosted_json.contains("enable_frosted_prepass_feeds") ||
        !frosted_json.at("enable_frosted_prepass_feeds").is_boolean())
    {
        out_error = "snapshot.frosted.enable_frosted_prepass_feeds must be a boolean.";
        return nullptr;
    }
    if (!frosted_json.contains("blur_source_mode") ||
        !frosted_json.at("blur_source_mode").is_number_unsigned())
    {
        out_error = "snapshot.frosted.blur_source_mode must be an unsigned number.";
        return nullptr;
    }
    if (!frosted_json.contains("full_fog_mode") ||
        !frosted_json.at("full_fog_mode").is_boolean())
    {
        out_error = "snapshot.frosted.full_fog_mode must be a boolean.";
        return nullptr;
    }
    if (!frosted_json.contains("world_prepass_panels") ||
        !frosted_json.at("world_prepass_panels").is_array())
    {
        out_error = "snapshot.frosted.world_prepass_panels must be an array.";
        return nullptr;
    }
    if (!frosted_json.contains("overlay_prepass_panels") ||
        !frosted_json.at("overlay_prepass_panels").is_array())
    {
        out_error = "snapshot.frosted.overlay_prepass_panels must be an array.";
        return nullptr;
    }

    auto snapshot = std::make_shared<FrostedGlassStateSnapshot>();
    static_cast<ModelViewerStateSnapshot&>(*snapshot) = *base_snapshot;
    snapshot->enable_panel_input_state_machine =
        frosted_json.at("enable_panel_input_state_machine").get<bool>();
    snapshot->enable_frosted_prepass_feeds =
        frosted_json.at("enable_frosted_prepass_feeds").get<bool>();
    snapshot->blur_source_mode = frosted_json.at("blur_source_mode").get<unsigned>();
    snapshot->full_fog_mode = frosted_json.at("full_fog_mode").get<bool>();

    for (const auto& panel_json : frosted_json.at("world_prepass_panels"))
    {
        if (!panel_json.is_object())
        {
            out_error = "snapshot.frosted.world_prepass_panels items must be objects.";
            return nullptr;
        }

        RendererSystemFrostedPanelProducer::WorldPanelPrepassItem panel{};
        if (!panel_json.contains("world_center") ||
            !ReadVec3(panel_json.at("world_center"), panel.world_center, out_error))
        {
            if (out_error.empty())
            {
                out_error = "snapshot.frosted.world_prepass_panels.world_center is invalid.";
            }
            return nullptr;
        }
        if (!panel_json.contains("world_axis_u") ||
            !ReadVec3(panel_json.at("world_axis_u"), panel.world_axis_u, out_error))
        {
            if (out_error.empty())
            {
                out_error = "snapshot.frosted.world_prepass_panels.world_axis_u is invalid.";
            }
            return nullptr;
        }
        if (!panel_json.contains("world_axis_v") ||
            !ReadVec3(panel_json.at("world_axis_v"), panel.world_axis_v, out_error))
        {
            if (out_error.empty())
            {
                out_error = "snapshot.frosted.world_prepass_panels.world_axis_v is invalid.";
            }
            return nullptr;
        }
        if (!panel_json.contains("layer_order") || !panel_json.at("layer_order").is_number() ||
            !panel_json.contains("depth_policy") || !panel_json.at("depth_policy").is_number_unsigned() ||
            !panel_json.contains("shape_type") || !panel_json.at("shape_type").is_number_unsigned() ||
            !panel_json.contains("custom_shape_index") || !panel_json.at("custom_shape_index").is_number() ||
            !panel_json.contains("corner_radius") || !panel_json.at("corner_radius").is_number() ||
            !panel_json.contains("panel_alpha") || !panel_json.at("panel_alpha").is_number())
        {
            out_error = "snapshot.frosted.world_prepass_panels scalar fields are invalid.";
            return nullptr;
        }

        panel.layer_order = panel_json.at("layer_order").get<float>();
        panel.depth_policy = static_cast<RendererSystemFrostedGlass::PanelDepthPolicy>(
            panel_json.at("depth_policy").get<unsigned>());
        panel.shape_type = static_cast<RendererSystemFrostedGlass::PanelShapeType>(
            panel_json.at("shape_type").get<unsigned>());
        panel.custom_shape_index = panel_json.at("custom_shape_index").get<float>();
        panel.corner_radius = panel_json.at("corner_radius").get<float>();
        panel.panel_alpha = panel_json.at("panel_alpha").get<float>();
        snapshot->world_prepass_panels.push_back(panel);
    }

    for (const auto& panel_json : frosted_json.at("overlay_prepass_panels"))
    {
        if (!panel_json.is_object())
        {
            out_error = "snapshot.frosted.overlay_prepass_panels items must be objects.";
            return nullptr;
        }

        RendererSystemFrostedPanelProducer::OverlayPanelPrepassItem panel{};
        if (!panel_json.contains("center_uv") ||
            !ReadVec2(panel_json.at("center_uv"), panel.center_uv, out_error))
        {
            if (out_error.empty())
            {
                out_error = "snapshot.frosted.overlay_prepass_panels.center_uv is invalid.";
            }
            return nullptr;
        }
        if (!panel_json.contains("half_size_uv") ||
            !ReadVec2(panel_json.at("half_size_uv"), panel.half_size_uv, out_error))
        {
            if (out_error.empty())
            {
                out_error = "snapshot.frosted.overlay_prepass_panels.half_size_uv is invalid.";
            }
            return nullptr;
        }
        if (!panel_json.contains("layer_order") || !panel_json.at("layer_order").is_number() ||
            !panel_json.contains("shape_type") || !panel_json.at("shape_type").is_number_unsigned() ||
            !panel_json.contains("custom_shape_index") || !panel_json.at("custom_shape_index").is_number() ||
            !panel_json.contains("corner_radius") || !panel_json.at("corner_radius").is_number() ||
            !panel_json.contains("panel_alpha") || !panel_json.at("panel_alpha").is_number())
        {
            out_error = "snapshot.frosted.overlay_prepass_panels scalar fields are invalid.";
            return nullptr;
        }

        panel.layer_order = panel_json.at("layer_order").get<float>();
        panel.shape_type = static_cast<RendererSystemFrostedGlass::PanelShapeType>(
            panel_json.at("shape_type").get<unsigned>());
        panel.custom_shape_index = panel_json.at("custom_shape_index").get<float>();
        panel.corner_radius = panel_json.at("corner_radius").get<float>();
        panel.panel_alpha = panel_json.at("panel_alpha").get<float>();
        snapshot->overlay_prepass_panels.push_back(panel);
    }

    return snapshot;
}

bool DemoAppModelViewerFrostedGlass::RebuildRenderRuntimeObjects()
{
    m_frosted_glass.reset();
    m_frosted_panel_producer.reset();

    if (!RebuildModelViewerBaseRuntimeObjects())
    {
        return false;
    }

    m_frosted_glass = std::make_shared<RendererSystemFrostedGlass>(m_scene, m_lighting);
    m_frosted_panel_producer = std::make_shared<RendererSystemFrostedPanelProducer>(m_frosted_glass);
    {
        RendererSystemFrostedGlass::FrostedGlassPanelDesc panel_desc{};
        panel_desc.center_uv = {0.5f, 0.52f};
        panel_desc.half_size_uv = {0.30f, 0.20f};
        panel_desc.corner_radius = 0.03f;
        panel_desc.blur_sigma = 8.5f;
        panel_desc.blur_strength = 0.92f;
        panel_desc.rim_intensity = 0.03f;
        panel_desc.tint_color = {0.94f, 0.97f, 1.0f};
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

    m_systems.push_back(m_frosted_panel_producer);
    m_systems.push_back(m_frosted_glass);
    return FinalizeModelViewerRuntimeObjects(m_frosted_glass);
}

bool DemoAppModelViewerFrostedGlass::InitInternal(const std::vector<std::string>& arguments)
{
    RETURN_IF_FALSE(RebuildRenderRuntimeObjects())

    const bool configured = ConfigureRegressionRunFromArguments(arguments);
    RefreshImportableRegressionCaseList();
    return configured;
}

void DemoAppModelViewerFrostedGlass::UpdateFrostedPanelPrepassFeeds(float timeline_seconds)
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

bool DemoAppModelViewerFrostedGlass::ConfigureRegressionRunFromArguments(const std::vector<std::string>& arguments)
{
    std::string suite_path_arg{};
    std::string output_root_arg{};
    bool enable_regression = false;
    bool enable_renderdoc_capture = false;
    bool renderdoc_required = false;

    const std::string suite_prefix = "-regression-suite=";
    const std::string output_prefix = "-regression-output=";
    for (const auto& argument : arguments)
    {
        if (argument == "-regression")
        {
            enable_regression = true;
            continue;
        }
        if (argument == "-renderdoc-capture")
        {
            enable_renderdoc_capture = true;
            continue;
        }
        if (argument == "-renderdoc-required")
        {
            renderdoc_required = true;
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
    m_regression_force_renderdoc_capture = enable_renderdoc_capture;
    m_regression_renderdoc_required = renderdoc_required;
    m_regression_finished = false;
    m_regression_case_active = false;
    m_regression_case_index = 0;
    m_regression_case_start_frame = 0;
    m_regression_case_last_elapsed_frames = 0;
    ResetActiveRegressionRenderDocCaptureState();
    m_regression_case_results.clear();
    m_regression_last_summary_path.clear();
    ResetRegressionPerfAccumulator();

    const bool suite_requests_renderdoc =
        m_regression_suite.default_capture_renderdoc ||
        std::any_of(
            m_regression_suite.cases.begin(),
            m_regression_suite.cases.end(),
            [](const Regression::CaseConfig& case_config) { return case_config.capture.capture_renderdoc; });
    const bool should_enable_renderdoc_runtime = m_regression_force_renderdoc_capture || suite_requests_renderdoc;
    if (m_render_graph)
    {
        std::string renderdoc_status{};
        if (!m_render_graph->ConfigureRenderDocCapture(
                should_enable_renderdoc_runtime,
                m_regression_renderdoc_required,
                renderdoc_status))
        {
            LOG_FORMAT_FLUSH("[Regression] RenderDoc setup failed: %s\n", renderdoc_status.c_str());
            return false;
        }

        if (should_enable_renderdoc_runtime)
        {
            LOG_FORMAT_FLUSH(
                "[Regression] RenderDoc capture requested (%s).\n",
                renderdoc_status.empty() ? "no status" : renderdoc_status.c_str());
        }
    }

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

void DemoAppModelViewerFrostedGlass::ResetRegressionPerfAccumulator()
{
    m_regression_perf_accumulator = RegressionPerfAccumulator{};
}

std::string DemoAppModelViewerFrostedGlass::BuildRegressionCasePrefix(unsigned case_index, const std::string& case_id) const
{
    std::ostringstream case_prefix_stream;
    case_prefix_stream << std::setfill('0') << std::setw(3) << case_index << "_" << SanitizeFileName(case_id);
    return case_prefix_stream.str();
}

void DemoAppModelViewerFrostedGlass::ResetActiveRegressionRenderDocCaptureState()
{
    m_regression_case_renderdoc_requested = false;
    m_regression_case_renderdoc_frame_index = 0ull;
    m_regression_case_renderdoc_requested_path.clear();
    m_regression_case_renderdoc_request_error.clear();
}

void DemoAppModelViewerFrostedGlass::TryQueueRegressionRenderDocCapture(
    const Regression::CaseConfig& case_config,
    unsigned long long elapsed_frames)
{
    if (!m_render_graph)
    {
        return;
    }

    const bool should_capture_renderdoc =
        ShouldCaptureRegressionRenderDoc(case_config, m_regression_force_renderdoc_capture);
    if (!should_capture_renderdoc || m_regression_case_renderdoc_requested)
    {
        return;
    }

    const unsigned long long target_elapsed_frames =
        GetRegressionRenderDocTargetElapsedFrames(case_config, m_regression_force_renderdoc_capture);
    if (target_elapsed_frames == 0ull || (elapsed_frames + 1ull) != target_elapsed_frames)
    {
        return;
    }

    const std::string case_prefix = BuildRegressionCasePrefix(
        static_cast<unsigned>(m_regression_case_index + 1u),
        case_config.id);
    const std::filesystem::path renderdoc_path = m_regression_output_root / "cases" / (case_prefix + ".rdc");
    const unsigned long long target_frame_index = m_render_graph->GetCurrentFrameIndex();

    std::string request_error{};
    if (!m_render_graph->RequestRenderDocCaptureForCurrentFrame(renderdoc_path, request_error))
    {
        m_regression_case_renderdoc_requested = false;
        m_regression_case_renderdoc_frame_index = target_frame_index;
        m_regression_case_renderdoc_requested_path = renderdoc_path;
        m_regression_case_renderdoc_request_error = request_error;
        LOG_FORMAT_FLUSH(
            "[Regression] RenderDoc capture request failed for case '%s': %s\n",
            case_config.id.c_str(),
            request_error.c_str());
        return;
    }

    m_regression_case_renderdoc_requested = true;
    m_regression_case_renderdoc_frame_index = target_frame_index;
    m_regression_case_renderdoc_requested_path = renderdoc_path;
    m_regression_case_renderdoc_request_error.clear();
    LOG_FORMAT_FLUSH(
        "[Regression] RenderDoc capture armed for case '%s' at frame %llu (elapsed=%llu offset=%u).\n",
        case_config.id.c_str(),
        static_cast<unsigned long long>(m_regression_case_renderdoc_frame_index),
        static_cast<unsigned long long>(target_elapsed_frames),
        case_config.capture.renderdoc_capture_frame_offset);
}

void DemoAppModelViewerFrostedGlass::AccumulateRegressionPerf(const RendererInterface::RenderGraph::FrameStats& frame_stats)
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

    if (m_render_graph)
    {
        const auto& frame_timing = m_render_graph->GetLastFrameTimingBreakdown();
        if (frame_timing.valid)
        {
            m_regression_perf_accumulator.frame_timing_valid_count += 1u;
            m_regression_perf_accumulator.frame_total_sum_ms += frame_timing.frame_total_ms;
            m_regression_perf_accumulator.execute_passes_sum_ms += frame_timing.execute_passes_ms;
            m_regression_perf_accumulator.non_pass_cpu_sum_ms += frame_timing.non_pass_cpu_ms;
            m_regression_perf_accumulator.frame_wait_total_sum_ms += frame_timing.frame_wait_total_ms;
            m_regression_perf_accumulator.wait_previous_frame_sum_ms += frame_timing.wait_previous_frame_ms;
            m_regression_perf_accumulator.acquire_command_list_sum_ms += frame_timing.acquire_command_list_ms;
            m_regression_perf_accumulator.acquire_swapchain_sum_ms += frame_timing.acquire_swapchain_ms;
            m_regression_perf_accumulator.execution_planning_sum_ms += frame_timing.execution_planning_ms;
            m_regression_perf_accumulator.blit_to_swapchain_sum_ms += frame_timing.blit_to_swapchain_ms;
            m_regression_perf_accumulator.submit_command_list_sum_ms += frame_timing.submit_command_list_ms;
            m_regression_perf_accumulator.present_call_sum_ms += frame_timing.present_call_ms;
            m_regression_perf_accumulator.prepare_frame_sum_ms += frame_timing.prepare_frame_ms;
            m_regression_perf_accumulator.finalize_submission_sum_ms += frame_timing.finalize_submission_ms;
        }
    }
}

bool DemoAppModelViewerFrostedGlass::ApplyRegressionCaseConfig(const Regression::CaseConfig& case_config, std::string& out_error)
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

bool DemoAppModelViewerFrostedGlass::StartRegressionCase(const RendererInterface::RenderGraph::FrameStats& frame_stats)
{
    if (m_regression_case_index >= m_regression_suite.cases.size())
    {
        return false;
    }

    const auto& case_config = m_regression_suite.cases[m_regression_case_index];
    const std::string case_id = case_config.id;
    ResetRegressionPerfAccumulator();
    ResetActiveRegressionRenderDocCaptureState();

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

bool DemoAppModelViewerFrostedGlass::CaptureWindowScreenshotPNG(const std::filesystem::path& file_path) const
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
    BOOL capture_success = FALSE;
#ifndef PW_RENDERFULLCONTENT
    constexpr UINT k_print_window_render_full_content = 0x00000002u;
#else
    constexpr UINT k_print_window_render_full_content = PW_RENDERFULLCONTENT;
#endif
    // Prefer client-area window rendering so regression screenshots are not polluted by overlapping desktop windows.
    capture_success = PrintWindow(hwnd, memory_dc, PW_CLIENTONLY | k_print_window_render_full_content);
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

    if (capture_success != TRUE)
    {
        return false;
    }

    return glTFImageIOUtil::Instance().SaveAsPNG(file_path.string(), rgba_data.data(), width, height);
}

bool DemoAppModelViewerFrostedGlass::WriteRegressionPassCsv(const RendererInterface::RenderGraph::FrameStats& frame_stats,
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

bool DemoAppModelViewerFrostedGlass::WriteRegressionPerfJson(const Regression::CaseConfig& case_config,
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
    summary["frame_timing_sample_count"] = m_regression_perf_accumulator.frame_timing_valid_count;
    if (m_regression_perf_accumulator.frame_timing_valid_count > 0u)
    {
        const unsigned timing_count = m_regression_perf_accumulator.frame_timing_valid_count;
        summary["frame_total_avg_ms"] = safe_avg(m_regression_perf_accumulator.frame_total_sum_ms, timing_count);
        summary["execute_passes_avg_ms"] = safe_avg(m_regression_perf_accumulator.execute_passes_sum_ms, timing_count);
        summary["non_pass_cpu_avg_ms"] = safe_avg(m_regression_perf_accumulator.non_pass_cpu_sum_ms, timing_count);
        summary["frame_wait_total_avg_ms"] = safe_avg(m_regression_perf_accumulator.frame_wait_total_sum_ms, timing_count);
        summary["wait_previous_frame_avg_ms"] = safe_avg(m_regression_perf_accumulator.wait_previous_frame_sum_ms, timing_count);
        summary["acquire_command_list_avg_ms"] = safe_avg(m_regression_perf_accumulator.acquire_command_list_sum_ms, timing_count);
        summary["acquire_swapchain_avg_ms"] = safe_avg(m_regression_perf_accumulator.acquire_swapchain_sum_ms, timing_count);
        summary["execution_planning_avg_ms"] = safe_avg(m_regression_perf_accumulator.execution_planning_sum_ms, timing_count);
        summary["blit_to_swapchain_avg_ms"] = safe_avg(m_regression_perf_accumulator.blit_to_swapchain_sum_ms, timing_count);
        summary["submit_command_list_avg_ms"] = safe_avg(m_regression_perf_accumulator.submit_command_list_sum_ms, timing_count);
        summary["present_call_avg_ms"] = safe_avg(m_regression_perf_accumulator.present_call_sum_ms, timing_count);
        summary["prepare_frame_avg_ms"] = safe_avg(m_regression_perf_accumulator.prepare_frame_sum_ms, timing_count);
        summary["finalize_submission_avg_ms"] = safe_avg(m_regression_perf_accumulator.finalize_submission_sum_ms, timing_count);
    }
    else
    {
        summary["frame_total_avg_ms"] = nullptr;
        summary["execute_passes_avg_ms"] = nullptr;
        summary["non_pass_cpu_avg_ms"] = nullptr;
        summary["frame_wait_total_avg_ms"] = nullptr;
        summary["wait_previous_frame_avg_ms"] = nullptr;
        summary["acquire_command_list_avg_ms"] = nullptr;
        summary["acquire_swapchain_avg_ms"] = nullptr;
        summary["execution_planning_avg_ms"] = nullptr;
        summary["blit_to_swapchain_avg_ms"] = nullptr;
        summary["submit_command_list_avg_ms"] = nullptr;
        summary["present_call_avg_ms"] = nullptr;
        summary["prepare_frame_avg_ms"] = nullptr;
        summary["finalize_submission_avg_ms"] = nullptr;
    }

    std::ofstream json_stream(file_path, std::ios::out | std::ios::trunc);
    if (!json_stream.is_open())
    {
        return false;
    }
    json_stream << summary.dump(2) << "\n";
    return true;
}

bool DemoAppModelViewerFrostedGlass::FinalizeRegressionCase(const RendererInterface::RenderGraph::FrameStats& frame_stats)
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
    const std::filesystem::path renderdoc_path = case_dir / (case_prefix + ".rdc");
    const bool should_capture_renderdoc =
        ShouldCaptureRegressionRenderDoc(case_config, m_regression_force_renderdoc_capture);

    RegressionCaseResult result{};
    result.id = case_config.id;
    result.success = true;
    result.renderdoc_capture_keep_on_success = case_config.capture.keep_renderdoc_on_success;

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

    if (should_capture_renderdoc)
    {
        result.renderdoc_capture_frame_index = m_regression_case_renderdoc_frame_index;
        result.renderdoc_capture_path = renderdoc_path.string();
        if (!m_regression_case_renderdoc_request_error.empty())
        {
            result.success = false;
            result.renderdoc_capture_success = false;
            result.renderdoc_capture_error = m_regression_case_renderdoc_request_error;
            result.error += "failed_to_arm_renderdoc_capture;";
        }
        else if (!m_regression_case_renderdoc_requested || m_regression_case_renderdoc_frame_index == 0ull)
        {
            result.success = false;
            result.renderdoc_capture_success = false;
            result.renderdoc_capture_error = "RenderDoc capture was requested by the case but was never armed.";
            result.error += "missing_renderdoc_capture_request;";
        }
        else if (!m_render_graph)
        {
            result.success = false;
            result.renderdoc_capture_success = false;
            result.renderdoc_capture_error = "Render graph is unavailable while finalizing RenderDoc capture.";
            result.error += "missing_render_graph_for_renderdoc;";
        }
        else
        {
            const unsigned long long actual_frame_index = m_render_graph->GetLastRenderDocCaptureFrameIndex();
            const std::string actual_capture_path = m_render_graph->GetLastRenderDocCapturePath();
            if (actual_frame_index != 0ull)
            {
                result.renderdoc_capture_frame_index = actual_frame_index;
            }
            if (!actual_capture_path.empty())
            {
                result.renderdoc_capture_path = actual_capture_path;
            }

            if (!m_render_graph->WasLastRenderDocCaptureSuccessful())
            {
                result.success = false;
                result.renderdoc_capture_success = false;
                result.renderdoc_capture_error = m_render_graph->GetLastRenderDocCaptureError();
                result.error += "failed_to_capture_renderdoc;";
            }
            else if (actual_frame_index != m_regression_case_renderdoc_frame_index)
            {
                result.success = false;
                result.renderdoc_capture_success = false;
                result.renderdoc_capture_error =
                    "RenderDoc capture frame mismatch. Expected frame " +
                    std::to_string(m_regression_case_renderdoc_frame_index) +
                    ", got " +
                    std::to_string(actual_frame_index) + ".";
                result.error += "renderdoc_capture_frame_mismatch;";
            }
            else if (result.renderdoc_capture_path.empty())
            {
                result.renderdoc_capture_success = false;
                result.success = false;
                result.renderdoc_capture_error = "RenderDoc reported success but did not return a capture path.";
                result.error += "missing_renderdoc_capture_path;";
            }
            else
            {
                result.renderdoc_capture_success = true;
                result.renderdoc_capture_retained = true;
            }
        }
    }

    if (result.renderdoc_capture_success &&
        result.success &&
        !case_config.capture.keep_renderdoc_on_success &&
        !result.renderdoc_capture_path.empty())
    {
        std::error_code remove_error{};
        std::error_code exists_error{};
        const std::filesystem::path retained_capture_path = result.renderdoc_capture_path;
        const bool removed = std::filesystem::remove(retained_capture_path, remove_error);
        const bool still_exists = std::filesystem::exists(retained_capture_path, exists_error);
        if (removed || (!still_exists && !exists_error))
        {
            result.renderdoc_capture_retained = false;
            result.renderdoc_capture_path.clear();
        }
        else if (remove_error)
        {
            LOG_FORMAT_FLUSH(
                "[Regression] RenderDoc capture retention cleanup failed for case '%s': %s\n",
                case_config.id.c_str(),
                remove_error.message().c_str());
        }
    }

    ResetActiveRegressionRenderDocCaptureState();
    m_regression_case_results.push_back(result);

    LOG_FORMAT_FLUSH("[Regression] Case %u/%u done: %s (%s)\n",
        static_cast<unsigned>(m_regression_case_index + 1u),
        static_cast<unsigned>(m_regression_suite.cases.size()),
        case_config.id.c_str(),
        result.success ? "OK" : "FAILED");
    return result.success;
}

bool DemoAppModelViewerFrostedGlass::FinalizeRegressionRun()
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
    summary["renderdoc_capture_enabled"] = m_render_graph ? m_render_graph->IsRenderDocCaptureEnabled() : false;
    summary["renderdoc_capture_available"] = m_render_graph ? m_render_graph->IsRenderDocCaptureAvailable() : false;
    summary["renderdoc_capture_required"] = m_regression_renderdoc_required;
    summary["renderdoc_capture_forced"] = m_regression_force_renderdoc_capture;
    summary["case_count"] = m_regression_suite.cases.size();
    summary["result_count"] = m_regression_case_results.size();

    nlohmann::json case_results = nlohmann::json::array();
    unsigned failed_count = 0u;
    unsigned renderdoc_capture_success_count = 0u;
    unsigned renderdoc_capture_retained_count = 0u;
    for (const auto& result : m_regression_case_results)
    {
        if (!result.success)
        {
            failed_count += 1u;
        }
        if (result.renderdoc_capture_success)
        {
            renderdoc_capture_success_count += 1u;
        }
        if (result.renderdoc_capture_retained)
        {
            renderdoc_capture_retained_count += 1u;
        }
        nlohmann::json case_item{};
        case_item["id"] = result.id;
        case_item["success"] = result.success;
        case_item["screenshot_path"] = result.screenshot_path;
        case_item["pass_csv_path"] = result.pass_csv_path;
        case_item["perf_json_path"] = result.perf_json_path;
        case_item["renderdoc_capture_success"] = result.renderdoc_capture_success;
        case_item["renderdoc_capture_retained"] = result.renderdoc_capture_retained;
        case_item["renderdoc_capture_keep_on_success"] = result.renderdoc_capture_keep_on_success;
        case_item["renderdoc_capture_frame_index"] = result.renderdoc_capture_frame_index;
        case_item["renderdoc_capture_path"] = result.renderdoc_capture_path;
        case_item["renderdoc_capture_error"] = result.renderdoc_capture_error;
        case_item["error"] = result.error;
        case_results.push_back(std::move(case_item));
    }
    summary["failed_count"] = failed_count;
    summary["renderdoc_capture_success_count"] = renderdoc_capture_success_count;
    summary["renderdoc_capture_retained_count"] = renderdoc_capture_retained_count;
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

bool DemoAppModelViewerFrostedGlass::ExportCurrentRegressionCaseTemplate()
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
        {"default_capture_frames", 16},
        {"default_capture_renderdoc", false},
        {"default_renderdoc_capture_frame_offset", 0},
        {"default_keep_renderdoc_on_success", true}
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

void DemoAppModelViewerFrostedGlass::RefreshImportableRegressionCaseList()
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

bool DemoAppModelViewerFrostedGlass::DeleteSelectedImportableRegressionCaseJson()
{
    if (m_regression_import_selected_index < 0 ||
        m_regression_import_selected_index >= static_cast<int>(m_regression_import_case_entries.size()))
    {
        m_last_regression_case_import_status = "Delete failed: no importable case selected.";
        return false;
    }

    const RegressionImportCaseEntry selected_entry =
        m_regression_import_case_entries[static_cast<size_t>(m_regression_import_selected_index)];
    std::error_code fs_error;
    const bool exists = std::filesystem::exists(selected_entry.full_path, fs_error);
    if (fs_error)
    {
        m_last_regression_case_import_status =
            "Delete failed to query file state: " + selected_entry.full_path.string() +
            " (" + fs_error.message() + ")";
        LOG_FORMAT_FLUSH("[Regression] %s\n", m_last_regression_case_import_status.c_str());
        return false;
    }

    if (!exists)
    {
        RefreshImportableRegressionCaseList();
        m_last_regression_case_import_status =
            "Delete skipped: file no longer exists: " + selected_entry.full_path.string();
        LOG_FORMAT_FLUSH("[Regression] %s\n", m_last_regression_case_import_status.c_str());
        return false;
    }

    const bool removed = std::filesystem::remove(selected_entry.full_path, fs_error);
    if (!removed || fs_error)
    {
        m_last_regression_case_import_status =
            "Delete failed: " + selected_entry.full_path.string() +
            (fs_error ? (" (" + fs_error.message() + ")") : std::string{});
        LOG_FORMAT_FLUSH("[Regression] %s\n", m_last_regression_case_import_status.c_str());
        return false;
    }

    const std::string removed_path = selected_entry.full_path.string();
    if (!m_last_regression_case_import_path.empty() &&
        m_last_regression_case_import_path == removed_path)
    {
        m_last_regression_case_import_path.clear();
    }

    RefreshImportableRegressionCaseList();
    m_last_regression_case_import_status = "Deleted case JSON: " + removed_path;
    LOG_FORMAT_FLUSH("[Regression] %s\n", m_last_regression_case_import_status.c_str());
    return true;
}

bool DemoAppModelViewerFrostedGlass::ImportRegressionCaseFromJson(const std::filesystem::path& suite_path)
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

void DemoAppModelViewerFrostedGlass::TickRegressionAutomation()
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
    const unsigned long long finalize_elapsed_frames =
        GetRegressionRenderDocTargetElapsedFrames(case_config, m_regression_force_renderdoc_capture);
    if (case_config.capture.capture_perf &&
        elapsed_frames >= capture_begin &&
        elapsed_frames <= capture_end)
    {
        AccumulateRegressionPerf(frame_stats);
    }

    TryQueueRegressionRenderDocCapture(case_config, elapsed_frames);

    if (elapsed_frames >= finalize_elapsed_frames)
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

void DemoAppModelViewerFrostedGlass::DrawDebugUIInternal()
{
    if (ImGui::CollapsingHeader("Demo"))
    {
        DrawModelViewerBaseDebugUI();
        ImGui::Checkbox("Enable Panel Input State Machine", &m_enable_panel_input_state_machine);
        if (ImGui::Checkbox("Enable Frosted Prepass Feeds", &m_enable_frosted_prepass_feeds))
        {
            UpdateFrostedPanelPrepassFeeds(m_directional_light_elapsed_seconds);
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
        ImGui::SameLine();
        if (ImGui::Button("Delete Selected Case JSON"))
        {
            DeleteSelectedImportableRegressionCaseJson();
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
