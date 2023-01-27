#include "glTFWindow.h"
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32 1
#include <GLFW/glfw3native.h>
#include "../glTFRHI/RHIDX12Impl/glTFRHIDX12.h"

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
        //&& glTFRHIDX12::Running
        )
    {
        //glTFRHIDX12::Update();
        //glTFRHIDX12::Render();
        m_passManager->RenderAllPass();
        glfwPollEvents();
    }

    //glTFRHIDX12::WaitForPreviousFrame();
    glfwTerminate();
}

bool glTFWindow::InitDX12()
{
    /*
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
    */

    m_passManager.reset(new glTFRenderPassManager(*this));
    m_passManager->InitAllPass();
    
    return true;
}