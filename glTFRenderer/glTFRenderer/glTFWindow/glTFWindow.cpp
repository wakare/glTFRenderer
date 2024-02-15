#include "glTFWindow.h"

#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32 1
#include <imgui.h>
#include <GLFW/glfw3native.h>

#include "glTFGUI/glTFGUI.h"

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
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    
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
    if (glTFGUI::HandleKeyBoardEventThisFrame())
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
    if (glTFGUI::HandleMouseEventThisFrame())
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