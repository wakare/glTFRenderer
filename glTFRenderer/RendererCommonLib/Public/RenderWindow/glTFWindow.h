#pragma once
#include <GLFW/glfw3.h>
#include <Windows.h>
#include <functional>
#include <memory>

#include "RendererInputDevice.h"

class glTFWindow
{
    friend class glTFGUIRenderer;
    
public:
    struct LoopTiming
    {
        bool valid{false};
        unsigned long long frame_index{0};
        float loop_total_ms{0.0f};
        float loop_thread_cpu_ms{0.0f};
        float idle_wait_ms{0.0f};
        float tick_callback_ms{0.0f};
        float tick_callback_thread_cpu_ms{0.0f};
        float poll_events_ms{0.0f};
        float poll_events_thread_cpu_ms{0.0f};
        float non_tick_ms{0.0f};
    };

    static glTFWindow& Get();
    bool InitAndShowWindow();

    bool RegisterCallbackEventNative();
    
    void UpdateWindow();
    LoopTiming GetLastLoopTiming() const;
    int GetWindowRefreshRate() const;
    void RequestClose();
    bool IsCloseRequested() const;

    void SetWidth(int width);
    void SetHeight(int height);
    int GetWidth() const {return m_width; }
    int GetHeight() const {return m_height; }
    
    // Can get hwnd by raw window
    HWND GetHWND() const;
    GLFWwindow* GetGLFWWindow() const;
    bool NeedHandleInput() const;

    void SetInputHandleCallback(const std::function<bool()>& input_handle_function);
    void SetTickCallback(const std::function<void(unsigned long long)>& tick);
    void SetExitCallback(const std::function<void()>& exit);
    void SetInputManager(const std::shared_ptr<RendererInputDevice>& input_manager);
    
private:
    glTFWindow();

    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void WindowResizedCallback(GLFWwindow* window, int width, int height);
    
    GLFWwindow* m_glfw_window;
    int m_width;
    int m_height;
    
    std::shared_ptr<RendererInputDevice> m_input_control;
    std::function<void(unsigned long long)> m_tick_callback {nullptr};
    std::function<void()> m_exit_callback {nullptr};
    std::function<bool()> m_handle_input_event {nullptr};

    unsigned long long m_last_tick_time{0};
    LoopTiming m_last_loop_timing{};
};
