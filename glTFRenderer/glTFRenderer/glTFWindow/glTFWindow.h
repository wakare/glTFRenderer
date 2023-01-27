#pragma once
#include "../glTFRenderPass/glTFRenderPassManager.h"

struct GLFWwindow;

class glTFWindow
{
public:
    glTFWindow();
    bool InitAndShowWindow();
    void UpdateWindow();

    int GetWidth() const {return m_width; }
    int GetHeight() const {return m_height; }
    
    // Can get hwnd by raw window
    GLFWwindow* GetRawWindow() {return m_glfwWindow;}
    const GLFWwindow* GetRawWindow() const {return m_glfwWindow;}
    
private:
    bool InitDX12();
    
    GLFWwindow* m_glfwWindow;
    int m_width;
    int m_height;

    std::unique_ptr<glTFRenderPassManager> m_passManager;
};
