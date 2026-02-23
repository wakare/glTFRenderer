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
    m_tone_map = std::make_shared<RendererSystemToneMap>(m_frosted_glass);
    {
        RendererSystemFrostedGlass::FrostedGlassPanelDesc panel_desc{};
        panel_desc.center_uv = {0.5f, 0.52f};
        panel_desc.half_size_uv = {0.30f, 0.20f};
        panel_desc.corner_radius = 0.03f;
        panel_desc.blur_sigma = 3.0f;
        panel_desc.blur_strength = 0.72f;
        panel_desc.rim_intensity = 0.10f;
        panel_desc.tint_color = {0.92f, 0.96f, 1.0f};
        panel_desc.depth_weight_scale = 100.0f;
        panel_desc.shape_type = RendererSystemFrostedGlass::PanelShapeType::RoundedRect;
        panel_desc.edge_softness = 1.0f;
        panel_desc.thickness = 0.02f;
        panel_desc.refraction_strength = 1.2f;
        panel_desc.fresnel_intensity = 0.10f;
        panel_desc.fresnel_power = 5.0f;
        m_frosted_glass->AddPanel(panel_desc);
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
    }
}
