#pragma once
#include "../glTFRenderPass/glTFRenderPassManager.h"
#include "../glTFScene/glTFSceneGraph.h"

struct GLFWwindow;

class glTFWindow
{
public:
    static glTFWindow& Get();
    bool InitAndShowWindow();
    void UpdateWindow();

    int GetWidth() const {return m_width; }
    int GetHeight() const {return m_height; }
    
    // Can get hwnd by raw window
    GLFWwindow* GetRawWindow() {return m_glfwWindow;}
    const GLFWwindow* GetRawWindow() const {return m_glfwWindow;}
    
private:
    glTFWindow();
    bool InitDX12();
    
    GLFWwindow* m_glfwWindow;
    int m_width;
    int m_height;

    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    std::unique_ptr<glTFRenderPassManager> m_passManager;
    std::unique_ptr<glTFSceneGraph> m_sceneGraph;
    std::unique_ptr<glTFSceneView> m_sceneView;
};
