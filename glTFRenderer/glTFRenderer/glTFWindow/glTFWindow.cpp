#include "glTFWindow.h"
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32 1
#include <GLFW/glfw3native.h>

#include "../glTFRenderPass/glTFRenderPassTest.h"
#include "../glTFRHI/RHIDX12Impl/glTFRHIDX12.h"

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
    
    m_glfwWindow = glfwCreateWindow(m_width, m_height, "Hello World", NULL, NULL);
    if (!m_glfwWindow)
    {
        glfwTerminate();
        return false;
    }

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
        m_passManager->RenderAllPass();
#endif
        glfwPollEvents();
    }
#ifdef DEBUG_OLD_VERSION
    glTFRHIDX12::WaitForPreviousFrame();
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
    m_passManager.reset(new glTFRenderPassManager(*this));
    m_passManager->AddRenderPass(std::make_unique<glTFRenderPassTest>());
    m_passManager->InitAllPass();
#endif    
    return true;
}