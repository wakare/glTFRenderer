#include "RenderWindow/glTFWindow.h"

#define GLFW_EXPOSE_NATIVE_WIN32 1
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <functional>
#include "InputKeyMapping.h"

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

void glTFWindow::UpdateWindow()
{
    while (!glfwWindowShouldClose(m_glfw_window))
    {
        unsigned long long current_tick_time = 0;
        if (m_last_tick_time > 0)
        {
            current_tick_time = GetTickCount64() - m_last_tick_time;
        }
        m_last_tick_time = GetTickCount64();
        
        if (m_tick_callback)
        {
            m_tick_callback(current_tick_time);   
        }
        
        glfwPollEvents();
    }

    if (m_exit_callback)
    {
        m_exit_callback();   
    }
    
    glfwTerminate();
}

void glTFWindow::SetWidth(int width)
{
    m_width = width;
}

void glTFWindow::SetHeight(int height)
{
    m_height = height;
}

HWND glTFWindow::GetHWND() const
{
    return glfwGetWin32Window(m_glfw_window);
}

GLFWwindow* glTFWindow::GetGLFWWindow() const
{
    return m_glfw_window;
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

void glTFWindow::SetTickCallback(const std::function<void(unsigned long long)>& tick)
{
    m_tick_callback = tick;
}

void glTFWindow::SetExitCallback(const std::function<void()>& exit)
{
    m_exit_callback = exit;
}

void glTFWindow::SetInputManager(const std::shared_ptr<RendererInputDevice>& input_manager)
{
    m_input_control = input_manager;
}

InputDeviceButtonType ToDeviceButton(int button)
{
    switch (button)
    {
        case GLFW_MOUSE_BUTTON_LEFT:
        return InputDeviceButtonType::MOUSE_BUTTON_LEFT;
        case GLFW_MOUSE_BUTTON_RIGHT:
        return InputDeviceButtonType::MOUSE_BUTTON_RIGHT;
        case GLFW_MOUSE_BUTTON_MIDDLE:
        return InputDeviceButtonType::MOUSE_BUTTON_MIDDLE;
    }
    return InputDeviceButtonType::BUTTON_UNKNOWN;
}

void glTFWindow::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (!glTFWindow::Get().NeedHandleInput())
    {
        return;
    }

    const auto device_key = GLFWKeyToInputDeviceKey(key);
    if (device_key != InputDeviceKeyType::KEY_UNKNOWN)
    {
        if (action == GLFW_PRESS)
        {
            Get().m_input_control->RecordKeyPressed(device_key);
        }
        else if (action == GLFW_RELEASE)
        {
            Get().m_input_control->RecordKeyRelease(device_key);
        }    
    }
}

void glTFWindow::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (!glTFWindow::Get().NeedHandleInput())
    {
        return;
    }
    if (ToDeviceButton(button) != InputDeviceButtonType::BUTTON_UNKNOWN)
    {
        if (action == GLFW_PRESS)
        {
            Get().m_input_control->RecordMouseButtonPressed(ToDeviceButton(button));
        }
        else if (action == GLFW_RELEASE)
        {
            Get().m_input_control->RecordMouseButtonRelease(ToDeviceButton(button));
        }
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
