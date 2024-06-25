#include "RenderWindow/glTFWindow.h"

#define GLFW_EXPOSE_NATIVE_WIN32 1
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <functional>

glTFWindow::glTFWindow()
    : m_glfw_window(nullptr)
    , m_width(1920)
    , m_height(1080)
{
    
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
    
    // Do not create opengl context, otherwise swap chain creation will fail
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_glfw_window = glfwCreateWindow(m_width, m_height, "glTFRenderer v0.01      by jackjwang", NULL, NULL);
    if (!m_glfw_window)
    {
        glfwTerminate();
        return false;
    }
    
    return true;
}

bool glTFWindow::RegisterCallbackEventNative()
{
    glfwSetKeyCallback(m_glfw_window, KeyCallback);
    glfwSetMouseButtonCallback(m_glfw_window, MouseButtonCallback);
    glfwSetCursorPosCallback(m_glfw_window, CursorPosCallback);
    glfwSetFramebufferSizeCallback(m_glfw_window, WindowResizedCallback);
    
    return true;
}

void glTFWindow::UpdateWindow() const
{
    while (!glfwWindowShouldClose(m_glfw_window))
    {
        if (m_tick_callback)
        {
            m_tick_callback();   
        }
        
        glfwPollEvents();
    }

    if (m_exit_callback)
    {
        m_exit_callback();   
    }
    
    glfwTerminate();
}

HWND glTFWindow::GetHWND() const
{
    return glfwGetWin32Window(m_glfw_window);
}

bool glTFWindow::NeedHandleInput() const
{
    if (m_handle_input_event && m_handle_input_event())
    {
        return false;
    }

    return true;
}

void glTFWindow::SetInputHandleCallback(const std::function<bool()>& input_handle_function)
{
    m_handle_input_event = input_handle_function;
}

void glTFWindow::SetTickCallback(const std::function<void()>& tick)
{
    m_tick_callback = tick;
}

void glTFWindow::SetExitCallback(const std::function<void()>& exit)
{
    m_exit_callback = exit;
}

void glTFWindow::SetInputManager(const std::shared_ptr<glTFInputManager>& input_manager)
{
    m_input_control = input_manager;
}

void glTFWindow::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (!glTFWindow::Get().NeedHandleInput())
    {
        return;
    }
    
    if (action == GLFW_PRESS)
    {
        Get().m_input_control->RecordKeyPressed(key);
    }
    else if (action == GLFW_RELEASE)
    {
        Get().m_input_control->RecordKeyRelease(key);
    }
}

void glTFWindow::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (!glTFWindow::Get().NeedHandleInput())
    {
        return;
    }
    
    if (action == GLFW_PRESS)
    {
        Get().m_input_control->RecordMouseButtonPressed(button);
    }
    else if (action == GLFW_RELEASE)
    {
        Get().m_input_control->RecordMouseButtonRelease(button);
    }
}

void glTFWindow::CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{   
    Get().m_input_control->RecordCursorPos(xpos, ypos);
}

void glTFWindow::WindowResizedCallback(GLFWwindow* window, int width, int height)
{
    // TODO: resized handle
    Get().m_width = width;
    Get().m_height = height;
}
