#pragma once

class glTFWindow
{
public:
    glTFWindow();
    bool InitAndShowWindow();
    void UpdateWindow();

private:
    bool InitDX12();
    
    class GLFWwindow* glfw_window;
    int width;
    int height;
};
