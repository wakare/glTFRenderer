#include "glTFWindow.h"
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32 1
#include <GLFW/glfw3native.h>

#include "../glTFMaterial/glTFMaterialOpaque.h"
#include "../glTFRenderPass/glTFRenderPassLighting.h"
#include "../glTFRenderPass/glTFRenderPassMeshOpaque.h"
#include "../glTFScene/glTFSceneBox.h"
#include "../glTFScene/glTFCamera.h"

glTFWindow::glTFWindow()
    : m_glfwWindow(nullptr)
    , m_width(1920)
    , m_height(1080)
{
    
}

glTFInputControl::glTFInputControl()
    : m_cursorX(0.0)
    , m_cursorY(0.0)
{
    memset(m_keyStatePressed, 0, sizeof(m_keyStatePressed));
}

void glTFInputControl::RecordKeyPressed(int keyCode)
{
    m_keyStatePressed[keyCode] = true;
}

void glTFInputControl::RecordKeyRelease(int keyCode)
{
    m_keyStatePressed[keyCode] = false;
}

bool glTFInputControl::IsKeyPressed(int keyCode) const
{
    return m_keyStatePressed[keyCode];
}

double glTFInputControl::GetCursorX() const
{
    return m_cursorX;
}

double glTFInputControl::GetCursorY() const
{
    return m_cursorY;
}

void glTFInputControl::RecordCursorPos(double X, double Y)
{
    m_cursorX = X;
    m_cursorY = Y;
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
    glfwSetCursorPosCallback(m_glfwWindow, CursorPosCallback);
    
    // Create test scene with box
    m_sceneGraph = std::make_unique<glTFSceneGraph>(); 
    std::shared_ptr<glTFMaterialOpaque> boxAlbedoMaterial = std::make_shared<glTFMaterialOpaque>(std::make_shared<glTFMaterialTexture>("glTFResources/tiger.bmp"));
    
    std::unique_ptr<glTFSceneNode> boxSceneNode = std::make_unique<glTFSceneNode>();
    boxSceneNode->object = std::make_unique<glTFSceneBox>();
    boxSceneNode->object->GetTransform().position = {5.0f, 0.0f, 0.0f};
    auto* boxNode = boxSceneNode.get();
    boxNode->object->SetTickFunc([boxNode]()
    {
        auto* box = boxNode->object.get();
        box->GetTransform().rotation.x += 0.0001f;
        box->GetTransform().rotation.y += 0.0002f;
        box->GetTransform().rotation.z += 0.0003f;
        boxNode->renderStateDirty = true;
    });
    ((glTFSceneBox*)boxSceneNode->object.get())->SetMaterial(boxAlbedoMaterial);
    m_sceneGraph->AddSceneNode(std::move(boxSceneNode));

    std::unique_ptr<glTFSceneNode> boxSceneNode2 = std::make_unique<glTFSceneNode>();
    boxSceneNode2->object = std::make_unique<glTFSceneBox>();
    boxSceneNode2->object->GetTransform().position = {-5.0f, 0.0f, 0.0f};
    auto* boxNode2 = boxSceneNode2.get();
    boxNode2->object->SetTickFunc([boxNode2]()
    {
        auto* box = boxNode2->object.get();
        box->GetTransform().rotation.x += 0.0001f;
        box->GetTransform().rotation.y += 0.0002f;
        box->GetTransform().rotation.z += 0.0003f;
        boxNode2->renderStateDirty = true;
    });
    ((glTFSceneBox*)boxSceneNode2->object.get())->SetMaterial(boxAlbedoMaterial);
    m_sceneGraph->AddSceneNode(std::move(boxSceneNode2));
    
    std::unique_ptr<glTFCamera> camera = std::make_unique<glTFCamera>(45.0f, 800.0f, 600.0f, 0.1f, 1000.0f);
    camera->GetTransform() = glTFTransform::Identity();
    camera->GetTransform().position = {0.0f, 0.0f, -15.0f};
    
    std::unique_ptr<glTFSceneNode> cameraNode = std::make_unique<glTFSceneNode>();
    cameraNode->object = std::move(camera);
    m_sceneGraph->AddSceneNode(std::move(cameraNode));

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
        m_sceneGraph->Tick();
        m_passManager->UpdateScene();
        m_passManager->RenderAllPass();

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
    
    glm::fvec4 deltaPosition = {0.0f, 0.0f, 0.0f, 0.0f};
    switch (key)
    {
    case GLFW_KEY_W:
        deltaPosition.z += 0.1f;
        break;

    case GLFW_KEY_A:
        deltaPosition.x += 0.1f;
        break;

    case GLFW_KEY_S:
        deltaPosition.z -= 0.1f;
        break;

    case GLFW_KEY_D:
        deltaPosition.x -= 0.1f;
        break;
        
    case GLFW_KEY_Q:
        deltaPosition.y += 0.1f;
        break;

    case GLFW_KEY_E:
        deltaPosition.y -= 0.1f;
        break;
    }

    const auto cameras = Get().m_sceneGraph->GetSceneCameras();
    if (!cameras.empty())
    {
        deltaPosition = deltaPosition * cameras[0]->GetTransform().GetTransformInverseMatrix();
        
        cameras[0]->GetTransform().position += glm::fvec3(deltaPosition);
        cameras[0]->MarkDirty();
    }
}

void glTFWindow::CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    if (Get().m_inputControl.IsKeyPressed(GLFW_KEY_LEFT_CONTROL))
    {
        // Do rotation
        const auto cameras = Get().m_sceneGraph->GetSceneCameras();
        if (!cameras.empty())
        {
            cameras[0]->GetTransform().rotation.y += 0.001f * (Get().m_inputControl.GetCursorX() - xpos);
            cameras[0]->GetTransform().rotation.x -= 0.001f * (Get().m_inputControl.GetCursorY() - ypos);
            cameras[0]->MarkDirty();
        }
    }
    
    Get().m_inputControl.RecordCursorPos(xpos, ypos);
}
