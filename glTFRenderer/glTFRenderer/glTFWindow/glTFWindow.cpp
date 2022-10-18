#include "glTFWindow.h"
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32 1
#include <GLFW/glfw3native.h>
#include <../glTFRenderer/glTFRHI/glTFRHIDX12.h>

glTFWindow::glTFWindow()
    : glfw_window(nullptr)
    , width(800)
    , height(600)
{
    
}

bool glTFWindow::InitAndShowWindow()
{
    if (!glfwInit())
    {
        return false;
    }
    
    glfw_window = glfwCreateWindow(width, height, "Hello World", NULL, NULL);
    if (!glfw_window)
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
    while (!glfwWindowShouldClose(glfw_window) && glTFRHIDX12::Running)
    {
        glTFRHIDX12::Update();
        glTFRHIDX12::Render();
        glfwPollEvents();
    }

    glTFRHIDX12::WaitForPreviousFrame();
    glfwTerminate();
}

bool glTFWindow::InitDX12()
{
    // Use this handle to init dx12 context
    HWND hwnd = glfwGetWin32Window(glfw_window);
    if (!hwnd)
    {
        return false;
    }
    
    if (!glTFRHIDX12::InitD3D(width, height, hwnd, false))
    {
        return false;
    }
    
    return true;
}