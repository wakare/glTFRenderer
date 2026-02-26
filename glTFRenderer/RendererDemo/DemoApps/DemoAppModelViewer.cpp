#include "DemoAppModelViewer.h"

#include "RenderWindow/RendererInputDevice.h"
#include <glm/glm/gtx/norm.hpp>
#include <cmath>
#include <imgui/imgui.h>

#include "RendererCommon.h"
#include "RendererModule/RendererModuleCamera.h"
#include "RendererModule/RendererModuleSceneMesh.h"

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

        RendererSystemFrostedGlass::FrostedGlassPanelDesc external_overlay_panel_desc = panel_desc;
        external_overlay_panel_desc.center_uv = {0.62f, 0.48f};
        external_overlay_panel_desc.half_size_uv = {0.16f, 0.12f};
        external_overlay_panel_desc.corner_radius = 0.02f;
        external_overlay_panel_desc.blur_sigma = 11.0f;
        external_overlay_panel_desc.blur_strength = 0.96f;
        external_overlay_panel_desc.rim_intensity = 0.04f;
        external_overlay_panel_desc.tint_color = {0.92f, 0.97f, 1.0f};
        external_overlay_panel_desc.shape_type = RendererSystemFrostedGlass::PanelShapeType::ShapeMask;
        external_overlay_panel_desc.custom_shape_index = 2.0f;
        external_overlay_panel_desc.layer_order = 1.0f;
        external_overlay_panel_desc.depth_policy = RendererSystemFrostedGlass::PanelDepthPolicy::Overlay;
        m_frosted_panel_producer->SetOverlayPanelDesc(external_overlay_panel_desc);

        RendererSystemFrostedGlass::FrostedGlassPanelDesc external_world_panel_desc = panel_desc;
        external_world_panel_desc.world_space_mode = 1u;
        external_world_panel_desc.depth_policy = RendererSystemFrostedGlass::PanelDepthPolicy::SceneOcclusion;
        external_world_panel_desc.world_center = {0.0f, 1.20f, -0.90f};
        external_world_panel_desc.world_axis_u = {0.56f, 0.0f, 0.0f};
        external_world_panel_desc.world_axis_v = {0.0f, 0.34f, 0.0f};
        external_world_panel_desc.center_uv = {0.5f, 0.5f};
        external_world_panel_desc.half_size_uv = {0.22f, 0.14f};
        external_world_panel_desc.corner_radius = 0.02f;
        external_world_panel_desc.blur_sigma = 10.5f;
        external_world_panel_desc.blur_strength = 0.95f;
        external_world_panel_desc.layer_order = -0.5f;
        external_world_panel_desc.shape_type = RendererSystemFrostedGlass::PanelShapeType::RoundedRect;
        external_world_panel_desc.custom_shape_index = 0.0f;
        m_frosted_panel_producer->SetWorldPanelDesc(external_world_panel_desc);
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

void DemoAppModelViewer::DrawDebugUIInternal()
{
    if (ImGui::CollapsingHeader("Demo", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SliderFloat("Directional Light Speed", &m_directional_light_angular_speed_radians, 0.0f, 2.0f, "%.3f rad/s");
        ImGui::Checkbox("Enable Panel Input State Machine", &m_enable_panel_input_state_machine);
        if (m_frosted_glass)
        {
            ImGui::Text("Effective Frosted Panel Count: %u", m_frosted_glass->GetEffectivePanelCount());
        }
        ImGui::TextUnformatted("Panel state mapping: move mouse=Hover, LMB=Move, RMB=Grab, Ctrl+LMB=Scale.");
    }
}
