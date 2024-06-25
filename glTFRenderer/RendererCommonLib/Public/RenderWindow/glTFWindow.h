#pragma once
#include <GLFW/glfw3.h>
#include <Windows.h>
#include <functional>
#include <memory>

#include "glTFInputManager.h"

class glTFWindow
{
    friend class glTFGUI;
    
public:
    static glTFWindow& Get();
    bool InitAndShowWindow();

    bool RegisterCallbackEventNative();
    
    void UpdateWindow() const;

    int GetWidth() const {return m_width; }
    int GetHeight() const {return m_height; }
    
    // Can get hwnd by raw window
    HWND GetHWND() const;
    bool NeedHandleInput() const;

    void SetInputHandleCallback(const std::function<bool()>& input_handle_function);
    void SetTickCallback(const std::function<void()>& tick);
    void SetExitCallback(const std::function<void()>& exit);
    void SetInputManager(const std::shared_ptr<glTFInputManager>& input_manager);
    
private:
    glTFWindow();

    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void WindowResizedCallback(GLFWwindow* window, int width, int height);
    
    GLFWwindow* m_glfw_window;
    int m_width;
    int m_height;
    
    std::shared_ptr<glTFInputManager> m_input_control;
    std::function<void()> m_tick_callback {nullptr};
    std::function<void()> m_exit_callback {nullptr};
    std::function<bool()> m_handle_input_event {nullptr};
};
