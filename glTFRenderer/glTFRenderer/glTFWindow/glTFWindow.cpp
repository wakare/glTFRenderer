#include "glTFWindow.h"
#include "../glTFLight/glTFDirectionalLight.h"
#include "../glTFLight/glTFPointLight.h"
#include "../glTFMaterial/glTFMaterialOpaque.h"
#include "../glTFRenderPass/glTFRenderPassLighting.h"
#include "../glTFRenderPass/glTFRenderPassMeshOpaque.h"
#include "../glTFScene/glTFCamera.h"
#include "../glTFLoader/glTFLoader.h"

#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32 1
#include <GLFW/glfw3native.h>

glTFWindow::glTFWindow()
    : m_glfw_window(nullptr)
    , m_width(1920)
    , m_height(1080)
{
    
}

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

glTFWindow& glTFWindow::Get()
{
    static glTFWindow window;
    return window;
}

bool glTFWindow::InitAndShowWindow()
{
    if (!glfwInit())
    {
        return false;
    }
    
    m_glfw_window = glfwCreateWindow(m_width, m_height, "glTFRenderer v0.01      by jackjwang", NULL, NULL);
    if (!m_glfw_window)
    {
        glfwTerminate();
        return false;
    }

    glfwSetKeyCallback(m_glfw_window, KeyCallback);
    glfwSetMouseButtonCallback(m_glfw_window, MouseButtonCallback);
    glfwSetCursorPosCallback(m_glfw_window, CursorPosCallback);
    
    // Create test scene with box
    m_scene_graph = std::make_unique<glTFSceneGraph>();
    //RETURN_IF_FALSE(LoadSceneGraphFromFile("glTFResources\\Models\\BoxTextured\\BoxTextured.gltf"))
    //RETURN_IF_FALSE(LoadSceneGraphFromFile("glTFResources\\Models\\Box\\Box.gltf"))
    //RETURN_IF_FALSE(LoadSceneGraphFromFile("glTFResources\\Models\\Monster\\Monster.gltf"))
    //RETURN_IF_FALSE(LoadSceneGraphFromFile("glTFResources\\Models\\Buggy\\glTF\\Buggy.gltf"))
    RETURN_IF_FALSE(LoadSceneGraphFromFile("glTFResources\\Models\\Sponza\\glTF\\Sponza.gltf"))

    // Add camera
    std::unique_ptr<glTFSceneNode> cameraNode = std::make_unique<glTFSceneNode>();
    std::unique_ptr<glTFCamera> camera = std::make_unique<glTFCamera>(m_scene_graph->GetRootNode().m_finalTransform,
        45.0f, 800.0f, 600.0f, 0.1f, 1000.0f);
    
    cameraNode->m_objects.push_back(std::move(camera));
    m_scene_graph->AddSceneNode(std::move(cameraNode));

    // Add light
    std::unique_ptr<glTFSceneNode> directional_light_node = std::make_unique<glTFSceneNode>();
    std::unique_ptr<glTFDirectionalLight> directionalLight = std::make_unique<glTFDirectionalLight>(directional_light_node->m_transform);
    directionalLight->Rotate({45.0f, 0.0f, 0.0f});
    directionalLight->SetIntensity(1.0f);
    directionalLight->SetTickFunc([lightNode = directionalLight.get()]()
    {
        lightNode->RotateOffset({0.000f, 20.0f, 0.000f});
    });
    directional_light_node->m_objects.push_back(std::move(directionalLight));

    std::unique_ptr<glTFSceneNode> point_light_node = std::make_unique<glTFSceneNode>();
    std::unique_ptr<glTFPointLight> point_light = std::make_unique<glTFPointLight>(point_light_node->m_transform);
    point_light->Translate({0.0f, 1.0f, 0.0f});
    point_light->SetRadius(0.7f);
    point_light->SetFalloff(1.0f);
    point_light->SetIntensity(10000.0f);
    point_light_node->m_objects.push_back(std::move(point_light));

    m_scene_graph->AddSceneNode(std::move(directional_light_node));
    m_scene_graph->AddSceneNode(std::move(point_light_node));
    
    m_scene_view = std::make_unique<glTFSceneView>(*m_scene_graph);
    
    if (!InitRenderPass())
    {
        return false;
    }

    return true;
}

void glTFWindow::UpdateWindow()
{
    while (!glfwWindowShouldClose(m_glfw_window))
    {
        m_timer.RecordFrameBegin();
        const size_t delta_time_ms = m_timer.GetDeltaFrameTimeMs();
        
        m_input_control.TickSceneView(*m_scene_view, delta_time_ms);
        
        m_scene_graph->Tick(delta_time_ms);
        m_passManager->UpdateScene(delta_time_ms);
        m_passManager->RenderAllPass(delta_time_ms);

        glfwPollEvents();
    }

    m_passManager->ExitAllPass();
    glfwTerminate();
}

bool glTFWindow::InitRenderPass()
{
    m_passManager.reset(new glTFRenderPassManager(*this, *m_scene_view));
    m_passManager->InitAllPass();
    
    return true;
}

void glTFWindow::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS)
    {
        Get().m_input_control.RecordKeyPressed(key);
    }
    else if (action == GLFW_RELEASE)
    {
        Get().m_input_control.RecordKeyRelease(key);
    }
}

void glTFWindow::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (action == GLFW_PRESS)
    {
        Get().m_input_control.RecordMouseButtonPressed(button);
    }
    else if (action == GLFW_RELEASE)
    {
        Get().m_input_control.RecordMouseButtonRelease(button);
    }
}

void glTFWindow::CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{   
    Get().m_input_control.RecordCursorPos(xpos, ypos);
}

bool glTFWindow::LoadSceneGraphFromFile(const char* filePath) const
{
    glTFLoader loader;
    if (loader.LoadFile(filePath))
    {
        loader.Print();    
    }

    RETURN_IF_FALSE(m_scene_graph->Init(loader))

    return true;
}
