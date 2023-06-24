#include "glTFWindow.h"
#include "../glTFLight/glTFDirectionalLight.h"
#include "../glTFLight/glTFPointLight.h"
#include "../glTFMaterial/glTFMaterialOpaque.h"
#include "../glTFRenderPass/glTFRenderPassLighting.h"
#include "../glTFRenderPass/glTFRenderPassMeshOpaque.h"
#include "../glTFScene/glTFSceneBox.h"
#include "../glTFScene/glTFCamera.h"
#include "../glTFLoader/glTFLoader.h"
#include "../glTFScene/glTFSceneTriangleMesh.h"

#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32 1
#include <GLFW/glfw3native.h>

glTFWindow::glTFWindow()
    : m_glfwWindow(nullptr)
    , m_width(1920)
    , m_height(1080)
{
    
}

glTFTimer::glTFTimer()
    : m_deltaTick(0)
    , m_tick(GetTickCount64())
{
}

void glTFTimer::RecordFrameBegin()
{
    const size_t now = GetTickCount64();
    m_deltaTick = now - m_tick;
    m_tick = now;
}

size_t glTFTimer::GetDeltaFrameTimeMs() const
{
    return m_deltaTick;
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
    
    m_glfwWindow = glfwCreateWindow(m_width, m_height, "glTFRenderer v0.01      by jackjwang", NULL, NULL);
    if (!m_glfwWindow)
    {
        glfwTerminate();
        return false;
    }

    glfwSetKeyCallback(m_glfwWindow, KeyCallback);
    glfwSetMouseButtonCallback(m_glfwWindow, MouseButtonCallback);
    glfwSetCursorPosCallback(m_glfwWindow, CursorPosCallback);
    
    // Create test scene with box
    m_sceneGraph = std::make_unique<glTFSceneGraph>(); 
    RETURN_IF_FALSE(LoadSceneGraphFromFile("glTFResources\\Models\\Box\\Box.gltf"))
    //RETURN_IF_FALSE(LoadSceneGraphFromFile("glTFResources\\Models\\Monster\\Monster.gltf"))
    //RETURN_IF_FALSE(LoadSceneGraphFromFile("glTFResources\\Models\\Buggy\\glTF\\Buggy.gltf"))

    glTF_AABB::AABB sceneAABB;
    m_sceneGraph->TraverseNodes([&sceneAABB](const glTFSceneNode& node)
    {
        for (const auto& object : node.m_objects)
        {
            const glTF_AABB::AABB world_AABB = glTF_AABB::AABB::TransformAABB(node.m_finalTransform.m_matrix, object->GetAABB());
            sceneAABB.extend(world_AABB);
        }
        
        return true;
    });
    
    // Add camera
    std::unique_ptr<glTFSceneNode> cameraNode = std::make_unique<glTFSceneNode>();
    std::unique_ptr<glTFCamera> camera = std::make_unique<glTFCamera>(m_sceneGraph->GetRootNode().m_finalTransform,
        45.0f, 800.0f, 600.0f, 0.1f, 1000.0f);
    //camera->Translate({0.0f, 0.0f, -5.0f});
    const float observe_distance = 2 * sceneAABB.getLongestEdge();
    //camera->Translate(sceneAABB.getCenter());
    camera->Translate(sceneAABB.getCenter() - glm::vec3 {0.0f, 0.0f, observe_distance});
    camera->Observe(sceneAABB.getCenter(), observe_distance);
    
    cameraNode->m_objects.push_back(std::move(camera));
    m_sceneGraph->AddSceneNode(std::move(cameraNode));

    // Add light
    std::unique_ptr<glTFSceneNode> directionalLightNode = std::make_unique<glTFSceneNode>();
    std::unique_ptr<glTFDirectionalLight> directionalLight = std::make_unique<glTFDirectionalLight>(directionalLightNode->m_transform);
    directionalLight->Rotate({45.0f, 0.0f, 0.0f});
    directionalLight->SetIntensity(10.0f);
    directionalLight->SetTickFunc([lightNode = directionalLight.get()]()
    {
        lightNode->Rotate({0.0001f, 0.0002f, 0.0003f});
    });
    directionalLightNode->m_objects.push_back(std::move(directionalLight));

    std::unique_ptr<glTFSceneNode> pointLightNode = std::make_unique<glTFSceneNode>();
    std::unique_ptr<glTFPointLight> pointLight = std::make_unique<glTFPointLight>(pointLightNode->m_transform);
    pointLight->Translate({0.0f, 1.0f, 0.0f});
    pointLight->SetRadius(0.7f);
    pointLight->SetFalloff(1.0f);
    pointLight->SetIntensity(10000.0f);
    pointLightNode->m_objects.push_back(std::move(pointLight));

    m_sceneGraph->AddSceneNode(std::move(directionalLightNode));
    m_sceneGraph->AddSceneNode(std::move(pointLightNode));
    
    m_sceneView = std::make_unique<glTFSceneView>(*m_sceneGraph);
    
    if (!InitDX12())
    {
        return false;
    }

    return true;
}

void glTFWindow::UpdateWindow()
{
    while (!glfwWindowShouldClose(m_glfwWindow))
    {
        m_timer.RecordFrameBegin();
        const size_t deltaTimeInMs = m_timer.GetDeltaFrameTimeMs();
        
        m_sceneGraph->Tick(deltaTimeInMs);
        m_inputControl.TickSceneView(*m_sceneView, deltaTimeInMs);
        m_passManager->UpdateScene(deltaTimeInMs);
        m_passManager->RenderAllPass(deltaTimeInMs);

        glfwPollEvents();
    }

    m_passManager->ExitAllPass();
    glfwTerminate();
}

bool glTFWindow::InitDX12()
{
    m_passManager.reset(new glTFRenderPassManager(*this, *m_sceneView));
    m_passManager->AddRenderPass(std::make_unique<glTFRenderPassMeshOpaque>());
    m_passManager->AddRenderPass(std::make_unique<glTFRenderPassLighting>());
    m_passManager->InitAllPass();
    
    return true;
}

void glTFWindow::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS)
    {
        Get().m_inputControl.RecordKeyPressed(key);
    }
    else if (action == GLFW_RELEASE)
    {
        Get().m_inputControl.RecordKeyRelease(key);
    }
}

void glTFWindow::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (action == GLFW_PRESS)
    {
        Get().m_inputControl.RecordMouseButtonPressed(button);
    }
    else if (action == GLFW_RELEASE)
    {
        Get().m_inputControl.RecordMouseButtonRelease(button);
    }
}

void glTFWindow::CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{   
    Get().m_inputControl.RecordCursorPos(xpos, ypos);
}

bool glTFWindow::LoadSceneGraphFromFile(const char* filePath) const
{
    glTFLoader loader;
    const bool loaded = loader.LoadFile(filePath);
    if (loaded)
    {
        loader.Print();    
    }

    RETURN_IF_FALSE(m_sceneGraph->Init(loader))

    return true;
}
