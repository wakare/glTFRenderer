#include "RenderWindow/glTFWindow.h"

#define GLFW_EXPOSE_NATIVE_WIN32 1
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <algorithm>
#include <functional>
#include <chrono>
#include "InputKeyMapping.h"

namespace
{
    static float ToMilliseconds(const std::chrono::steady_clock::time_point& begin, const std::chrono::steady_clock::time_point& end)
    {
        return std::chrono::duration<float, std::milli>(end - begin).count();
    }

    static unsigned long long GetCurrentThreadCPUTime100ns()
    {
        FILETIME create_time{};
        FILETIME exit_time{};
        FILETIME kernel_time{};
        FILETIME user_time{};
        if (!GetThreadTimes(GetCurrentThread(), &create_time, &exit_time, &kernel_time, &user_time))
        {
            return 0ULL;
        }

        ULARGE_INTEGER kernel_value{};
        kernel_value.LowPart = kernel_time.dwLowDateTime;
        kernel_value.HighPart = kernel_time.dwHighDateTime;

        ULARGE_INTEGER user_value{};
        user_value.LowPart = user_time.dwLowDateTime;
        user_value.HighPart = user_time.dwHighDateTime;
        return kernel_value.QuadPart + user_value.QuadPart;
    }

    static float ToMillisecondsFrom100ns(unsigned long long begin_100ns, unsigned long long end_100ns)
    {
        if (end_100ns < begin_100ns)
        {
            return 0.0f;
        }
        return static_cast<float>(end_100ns - begin_100ns) * 0.0001f;
    }
}

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
    if (!m_glfw_initialized)
    {
        if (!glfwInit())
        {
            return false;
        }
        m_glfw_initialized = true;
    }
    
    // Do not create opengl context, otherwise swap chain creation will fail
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_glfw_window = glfwCreateWindow(m_width, m_height, "glTFRenderer v0.01      by jackjwang", NULL, NULL);
    if (!m_glfw_window)
    {
        return false;
    }

    m_last_tick_time = 0;
    m_last_loop_timing = {};
    
    return true;
}

void glTFWindow::ShutdownGLFW()
{
    if (m_glfw_window)
    {
        glfwDestroyWindow(m_glfw_window);
        m_glfw_window = nullptr;
    }

    m_tick_callback = {};
    m_exit_callback = {};
    m_handle_input_event = {};
    m_input_control.reset();
    m_last_tick_time = 0;
    m_last_loop_timing = {};

    if (m_glfw_initialized)
    {
        glfwTerminate();
        m_glfw_initialized = false;
    }
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
    if (!m_glfw_window)
    {
        return;
    }

    while (!glfwWindowShouldClose(m_glfw_window))
    {
        const auto loop_begin = std::chrono::steady_clock::now();
        const unsigned long long loop_cpu_begin = GetCurrentThreadCPUTime100ns();
        unsigned long long current_tick_time = 0;
        if (m_last_tick_time > 0)
        {
            current_tick_time = GetTickCount64() - m_last_tick_time;
        }
        m_last_tick_time = GetTickCount64();
        
        const auto tick_begin = std::chrono::steady_clock::now();
        const unsigned long long tick_cpu_begin = GetCurrentThreadCPUTime100ns();
        if (m_tick_callback)
        {
            m_tick_callback(current_tick_time);   
        }
        const unsigned long long tick_cpu_end = GetCurrentThreadCPUTime100ns();
        const auto tick_end = std::chrono::steady_clock::now();
        
        const auto poll_begin = std::chrono::steady_clock::now();
        const unsigned long long poll_cpu_begin = GetCurrentThreadCPUTime100ns();
        glfwPollEvents();
        const unsigned long long poll_cpu_end = GetCurrentThreadCPUTime100ns();
        const auto poll_end = std::chrono::steady_clock::now();

        const auto loop_end = std::chrono::steady_clock::now();
        const unsigned long long loop_cpu_end = GetCurrentThreadCPUTime100ns();
        m_last_loop_timing.valid = true;
        ++m_last_loop_timing.frame_index;
        m_last_loop_timing.loop_total_ms = ToMilliseconds(loop_begin, loop_end);
        m_last_loop_timing.loop_thread_cpu_ms = ToMillisecondsFrom100ns(loop_cpu_begin, loop_cpu_end);
        const float idle_wait_ms = m_last_loop_timing.loop_total_ms - m_last_loop_timing.loop_thread_cpu_ms;
        m_last_loop_timing.idle_wait_ms = idle_wait_ms > 0.0f ? idle_wait_ms : 0.0f;
        m_last_loop_timing.tick_callback_ms = ToMilliseconds(tick_begin, tick_end);
        m_last_loop_timing.tick_callback_thread_cpu_ms = ToMillisecondsFrom100ns(tick_cpu_begin, tick_cpu_end);
        m_last_loop_timing.poll_events_ms = ToMilliseconds(poll_begin, poll_end);
        m_last_loop_timing.poll_events_thread_cpu_ms = ToMillisecondsFrom100ns(poll_cpu_begin, poll_cpu_end);
        const float non_tick_ms =
            m_last_loop_timing.loop_total_ms -
            m_last_loop_timing.tick_callback_ms -
            m_last_loop_timing.poll_events_ms;
        m_last_loop_timing.non_tick_ms = non_tick_ms > 0.0f ? non_tick_ms : 0.0f;
    }

    if (m_exit_callback)
    {
        m_exit_callback();   
    }

    if (m_glfw_window)
    {
        glfwDestroyWindow(m_glfw_window);
        m_glfw_window = nullptr;
    }

    m_tick_callback = {};
    m_exit_callback = {};
    m_handle_input_event = {};
    m_input_control.reset();
    m_last_tick_time = 0;
    m_last_loop_timing = {};
}

glTFWindow::LoopTiming glTFWindow::GetLastLoopTiming() const
{
    return m_last_loop_timing;
}

int glTFWindow::GetWindowRefreshRate() const
{
    if (!m_glfw_window)
    {
        return 0;
    }

    int window_x = 0;
    int window_y = 0;
    int window_width = 0;
    int window_height = 0;
    glfwGetWindowPos(m_glfw_window, &window_x, &window_y);
    glfwGetWindowSize(m_glfw_window, &window_width, &window_height);
    const int window_left = window_x;
    const int window_top = window_y;
    const int window_right = window_x + (std::max)(1, window_width);
    const int window_bottom = window_y + (std::max)(1, window_height);

    int monitor_count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);
    GLFWmonitor* best_monitor = nullptr;
    int best_overlap = -1;
    for (int monitor_index = 0; monitor_index < monitor_count; ++monitor_index)
    {
        GLFWmonitor* monitor = monitors[monitor_index];
        if (!monitor)
        {
            continue;
        }

        int monitor_x = 0;
        int monitor_y = 0;
        glfwGetMonitorPos(monitor, &monitor_x, &monitor_y);
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (!mode)
        {
            continue;
        }

        const int monitor_left = monitor_x;
        const int monitor_top = monitor_y;
        const int monitor_right = monitor_x + mode->width;
        const int monitor_bottom = monitor_y + mode->height;
        const int overlap_width = (std::max)(0, (std::min)(window_right, monitor_right) - (std::max)(window_left, monitor_left));
        const int overlap_height = (std::max)(0, (std::min)(window_bottom, monitor_bottom) - (std::max)(window_top, monitor_top));
        const int overlap_area = overlap_width * overlap_height;
        if (overlap_area > best_overlap)
        {
            best_overlap = overlap_area;
            best_monitor = monitor;
        }
    }

    if (!best_monitor)
    {
        best_monitor = glfwGetPrimaryMonitor();
    }
    if (!best_monitor)
    {
        return 0;
    }

    const GLFWvidmode* best_mode = glfwGetVideoMode(best_monitor);
    return best_mode ? best_mode->refreshRate : 0;
}

void glTFWindow::RequestClose()
{
    if (m_glfw_window)
    {
        glfwSetWindowShouldClose(m_glfw_window, GLFW_TRUE);
    }
}

bool glTFWindow::IsCloseRequested() const
{
    return m_glfw_window ? glfwWindowShouldClose(m_glfw_window) == GLFW_TRUE : true;
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
    return m_glfw_window ? glfwGetWin32Window(m_glfw_window) : nullptr;
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
