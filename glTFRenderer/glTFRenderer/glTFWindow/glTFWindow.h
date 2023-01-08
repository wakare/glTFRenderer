#pragma once

class GLFWwindow;

class glTFWindow
{
public:
    glTFWindow();
    bool InitAndShowWindow();
    void UpdateWindow();

    // Can get hwnd by raw window
    GLFWwindow* GetRawWindow() {return m_glfwWindow;}
    const GLFWwindow* GetRawWindow() const {return m_glfwWindow;}
    
private:
    bool InitDX12();
    
    GLFWwindow* m_glfwWindow;
    int width;
    int height;
};
