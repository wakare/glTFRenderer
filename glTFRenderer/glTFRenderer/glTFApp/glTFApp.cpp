#include "glTFApp.h"

#include <Windows.h>
#include "glTFLight/glTFDirectionalLight.h"
#include "glTFLight/glTFPointLight.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassLighting.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassMeshOpaque.h"
#include "glTFLoader/glTFLoader.h"
#include "glTFWindow/glTFWindow.h"

glTFTimer::glTFTimer()
    : m_delta_tick(0)
    , m_tick(GetTickCount64())
{
}

void glTFTimer::RecordFrameBegin()
{
    const size_t now = GetTickCount64();
    m_delta_tick = now - m_tick;
    m_tick = now;
}

size_t glTFTimer::GetDeltaFrameTimeMs() const
{
    return m_delta_tick;
}

bool glTFApp::InitApp()
{
    m_input_manager.reset(new glTFInputManager);
    
    auto& window = glTFWindow::Get();
    
    //Register window callback with App
    window.SetTickCallback([this](){
        TickFrame();
    });

    window.SetExitCallback([this]()
    {
        ExitApp(); 
    });
    window.SetInputManager(m_input_manager);
    
    const unsigned width = window.GetWidth();
    const unsigned height = window.GetHeight();
    GLTF_CHECK(window.InitAndShowWindow());
    
     // Create test scene with filename
    m_scene_graph = std::make_unique<glTFSceneGraph>();
    char file_path [MAX_PATH] = {'\0'};
    const std::string file_name = "Sponza";
    snprintf(file_path, sizeof(file_path), "glTFResources\\Models\\%s\\glTF\\%s.gltf", file_name.c_str(), file_name.c_str());
    RETURN_IF_FALSE(LoadSceneGraphFromFile(file_path))

    // Add camera
    std::unique_ptr<glTFSceneNode> camera_node = std::make_unique<glTFSceneNode>();
    std::unique_ptr<glTFCamera> camera = std::make_unique<glTFCamera>(m_scene_graph->GetRootNode().m_finalTransform,
        90.0f, width, height, 0.001f, 1000.0f);
    
    camera_node->m_objects.push_back(std::move(camera));
    
    // Add light
    std::unique_ptr<glTFSceneNode> directional_light_node = std::make_unique<glTFSceneNode>();
    std::unique_ptr<glTFDirectionalLight> directionalLight = std::make_unique<glTFDirectionalLight>(directional_light_node->m_transform);
    directionalLight->Rotate({glm::radians(45.0f), 0.0f, 0.0f});
    directionalLight->SetIntensity(1.0f);
    directionalLight->SetTickFunc([lightNode = directionalLight.get()]()
    {
        lightNode->RotateOffset({0.0f, 0.001f, 0.0f});
    });
    directional_light_node->m_objects.push_back(std::move(directionalLight));
    m_scene_graph->AddSceneNode(std::move(directional_light_node));

    std::unique_ptr<glTFSceneNode> point_light_node = std::make_unique<glTFSceneNode>();
    std::unique_ptr<glTFPointLight> point_light = std::make_unique<glTFPointLight>(point_light_node->m_transform);
    point_light->Translate({0.0f, 5.0f, -5.0f});
    point_light->SetRadius(10.0f);
    point_light->SetFalloff(1.0f);
    point_light->SetIntensity(1.0f);
    point_light_node->m_objects.push_back(std::move(point_light));

    std::unique_ptr<glTFPointLight> point_light2 = std::make_unique<glTFPointLight>(point_light_node->m_transform);
    point_light2->Translate({0.0f, 1.0f, 0.0f});
    point_light2->SetRadius(5.0f);
    point_light2->SetFalloff(1.0f);
    point_light2->SetIntensity(1.0f);
    point_light_node->m_objects.push_back(std::move(point_light2));
    m_scene_graph->AddSceneNode(std::move(point_light_node));

    m_scene_graph->AddSceneNode(std::move(camera_node));
    
    m_scene_view = std::make_unique<glTFSceneView>(*m_scene_graph);
    m_pass_manager.reset(new glTFRenderPassManager(*m_scene_view));
    m_pass_manager->InitRenderPassManager(width, height, window.GetHWND());
    m_pass_manager->InitAllPass();
    
    return true;
}

void glTFApp::RunApp()
{
    glTFWindow::Get().UpdateWindow();
}

void glTFApp::TickFrame()
{
    m_timer.RecordFrameBegin();
    const size_t delta_time_ms = m_timer.GetDeltaFrameTimeMs();
        
    m_input_manager->TickSceneView(*m_scene_view, delta_time_ms);
    m_scene_graph->Tick(delta_time_ms);
    m_pass_manager->UpdateScene(delta_time_ms);
    m_pass_manager->RenderAllPass(delta_time_ms);
}

void glTFApp::ExitApp() const
{
    m_pass_manager->ExitAllPass();
}

bool glTFApp::LoadSceneGraphFromFile(const char* filePath) const
{
    glTFLoader loader;
    if (loader.LoadFile(filePath))
    {
        loader.Print();    
    }

    return m_scene_graph->Init(loader);
}
