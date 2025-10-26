#include "DemoAppModelViewer.h"

#include "RenderWindow/RendererInputDevice.h"
#include <glm/glm/gtx/norm.hpp>

#include "RendererCommon.h"
#include "RendererModule/RendererModuleCamera.h"
#include "RendererModule/RendererModuleSceneMesh.h"

void DemoAppModelViewer::TickFrameInternal(unsigned long long time_interval)
{
    auto& input_device = m_window->GetInputDevice();
    m_scene->UpdateInputDeviceInfo(input_device, time_interval);
    
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
    
    // Add test light
    LightInfo directional_light_info{};
    directional_light_info.type = Directional;
    directional_light_info.intensity = {1.0f, 1.0f, 1.0f};
    directional_light_info.position = {0.0f, -0.707f, -0.707f};
    directional_light_info.radius = 100000.0f;
    m_lighting->AddLight(directional_light_info);

    m_systems.push_back(m_scene);
    m_systems.push_back(m_lighting);
    
    // After registration all passes, compile graph and prepare for execution
    m_render_graph->CompileRenderPassAndExecute();

    return true;
}