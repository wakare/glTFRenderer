#pragma once

class glTFWindow
{
public:
    bool InitAndShowWindow();
    void UpdateWindow();

private:
    bool InitDX12();
    void RenderDX12();
    
    class GLFWwindow* glfw_window;
};
