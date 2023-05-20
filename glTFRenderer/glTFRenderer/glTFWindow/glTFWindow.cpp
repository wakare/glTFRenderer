#include "glTFWindow.h"
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32 1
#include <GLFW/glfw3native.h>

#include "../glTFMaterial/glTFMaterialOpaque.h"
#include "../glTFRenderPass/glTFRenderPassMeshOpaque.h"
#include "../glTFRHI/RHIDX12Impl/glTFRHIDX12.h"
#include "../glTFScene/glTFSceneBox.h"
#include "../glTFScene/glTFCamera.h"

//#define DEBUG_OLD_VERSION

glTFWindow::glTFWindow()
    : m_glfwWindow(nullptr)
    , m_width(800)
    , m_height(600)
{
    
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

    // Create test scene with box
    m_sceneGraph = std::make_unique<glTFSceneGraph>();
    std::shared_ptr<glTFMaterialTexture> boxMaterialTexture = std::make_shared<glTFMaterialTexture>("glTFResources/tiger.bmp"); 
    std::shared_ptr<glTFMaterialOpaque> boxAlbedoMaterial = std::make_shared<glTFMaterialOpaque>(boxMaterialTexture);
    
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
    while (!glfwWindowShouldClose(m_glfwWindow)
#ifdef DEBUG_OLD_VERSION
        && glTFRHIDX12::Running
#endif
        )
    {
#ifdef DEBUG_OLD_VERSION
        glTFRHIDX12::Update();
        glTFRHIDX12::Render();
#else
        m_sceneGraph->Tick();
        m_passManager->UpdateScene();
        m_passManager->RenderAllPass();
        
#endif
        glfwPollEvents();
    }
#ifdef DEBUG_OLD_VERSION
    glTFRHIDX12::WaitForPreviousFrame();
#else
    m_passManager->ExitAllPass();
#endif
    glfwTerminate();
}

bool glTFWindow::InitDX12()
{
#ifdef DEBUG_OLD_VERSION
    // Use this handle to init dx12 context
    HWND hwnd = glfwGetWin32Window(m_glfwWindow);
    if (!hwnd)
    {
        return false;
    }
    
    if (!glTFRHIDX12::InitD3D(m_width, m_height, hwnd, false))
    {
        return false;
    }

#else
    m_passManager.reset(new glTFRenderPassManager(*this, *m_sceneView));
    //m_passManager->AddRenderPass(std::make_unique<glTFRenderPassTest>());
    m_passManager->AddRenderPass(std::make_unique<glTFRenderPassMeshOpaque>());
    m_passManager->InitAllPass();
#endif    
    return true;
}