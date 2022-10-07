#include "glTFWindow.h"
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32 1
#include <GLFW/glfw3native.h>

bool glTFWindow::InitAndShowWindow()
{
    if (!glfwInit())
    {
        return false;
    }
    
    glfw_window = glfwCreateWindow(800, 600, "Hello World", NULL, NULL);
    if (!glfw_window)
    {
        glfwTerminate();
        return false;
    }
    
    return true;
}

void glTFWindow::UpdateWindow()
{
    while (!glfwWindowShouldClose(glfw_window))
    {
        RenderDX12();
        glfwPollEvents();
    }

    glfwTerminate();
}

bool glTFWindow::InitDX12()
{
    // Use this handle to init dx12 context
    HWND hwnd = glfwGetWin32Window(glfw_window); 

    
    
    return true;
}

void glTFWindow::RenderDX12()
{
    
}
