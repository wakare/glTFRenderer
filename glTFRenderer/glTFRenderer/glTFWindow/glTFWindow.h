#pragma once
#include <GLFW/glfw3.h>

#include "../glTFRenderPass/glTFRenderPassManager.h"
#include "../glTFScene/glTFSceneGraph.h"
#include "glTFInputManager.h"

class glTFLoader;
struct GLFWwindow;

class glTFTimer
{
public:
    glTFTimer();
    void RecordFrameBegin();
    size_t GetDeltaFrameTimeMs() const;

private:
    size_t m_delta_tick;
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
    GLFWwindow* GetRawWindow() {return m_glfw_window;}
    const GLFWwindow* GetRawWindow() const {return m_glfw_window;}
    
private:
    glTFWindow();
    bool InitScene();
    bool InitRenderPass();

    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);

    bool LoadSceneGraphFromFile(const char* filePath) const;
    
    GLFWwindow* m_glfw_window;
    int m_width;
    int m_height;
    
    std::unique_ptr<glTFRenderPassManager> m_pass_manager;
    std::unique_ptr<glTFSceneGraph> m_scene_graph;
    std::unique_ptr<glTFSceneView> m_scene_view;

    glTFInputManager m_input_control;
    glTFTimer m_timer;
};
