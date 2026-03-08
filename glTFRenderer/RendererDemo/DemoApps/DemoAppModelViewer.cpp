#include "DemoAppModelViewer.h"

#include "RenderWindow/RendererInputDevice.h"
#include <glm/glm/gtx/norm.hpp>
#include <algorithm>
#include <cmath>
#include <imgui/imgui.h>

namespace
{
    constexpr float DIRECTIONAL_LIGHT_DIRECTION_EPSILON_SQ = 1.0e-8f;

    nlohmann::json ToJson(const glm::fvec3& value)
    {
        return nlohmann::json::array({value.x, value.y, value.z});
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
    return snapshot;
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
    m_lighting.reset();
    m_tone_map.reset();

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
    m_lighting = std::make_shared<RendererSystemLighting>(*m_resource_manager, m_scene);

    LightInfo directional_light_info{};
    directional_light_info.type = Directional;
    directional_light_info.intensity = {1.0f, 1.0f, 1.0f};
    directional_light_info.position = {0.0f, -0.707f, -0.707f};
    directional_light_info.radius = 100000.0f;
    m_directional_light_info = directional_light_info;
    m_directional_light_index = m_lighting->AddLight(m_directional_light_info);

    m_systems.push_back(m_scene);
    m_systems.push_back(m_lighting);
    return true;
}

bool DemoAppModelViewer::FinalizeModelViewerRuntimeObjects(
    const std::shared_ptr<RendererSystemFrostedGlass>& tone_map_frosted_input)
{
    m_tone_map = std::make_shared<RendererSystemToneMap>(tone_map_frosted_input, m_lighting, m_scene);
    m_systems.push_back(m_tone_map);
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
    (void)arguments;
    return RebuildRenderRuntimeObjects();
}

void DemoAppModelViewer::TickFrameInternal(unsigned long long time_interval)
{
    UpdateModelViewerFrame(time_interval);

    if (m_window)
    {
        m_window->GetInputDevice().TickFrame(time_interval);
    }
}

void DemoAppModelViewer::DrawModelViewerBaseDebugUI()
{
    ImGui::SliderFloat("Directional Light Speed", &m_directional_light_angular_speed_radians, 0.0f, 2.0f, "%.3f rad/s");
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
    return snapshot;
}
