#pragma once
#include <GLFW/glfw3.h>

#include "../glTFRenderPass/glTFRenderPassManager.h"
#include "../glTFScene/glTFSceneGraph.h"
#include "glTFInputManager.h"

struct GLFWwindow;

class glTFTimer
{
public:
    glTFTimer();
    void RecordFrameBegin();
    size_t GetDeltaFrameTimeMs() const;

private:
    size_t m_deltaTick;
    size_t m_tick;
};

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

    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);

    bool LoadSceneGraphFromFile(const char* filePath);
    
    GLFWwindow* m_glfwWindow;
    int m_width;
    int m_height;
    
    std::unique_ptr<glTFRenderPassManager> m_passManager;
    std::unique_ptr<glTFSceneGraph> m_sceneGraph;
    std::unique_ptr<glTFSceneView> m_sceneView;

    glTFInputManager m_inputControl;
    glTFTimer m_timer;
};
