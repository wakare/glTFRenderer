#include "glTFWindow.h"
#include <GLFW/glfw3.h>

bool glTFWindow::InitWindow()
{
    if (!glfwInit())
    {
        return false;
    }

    return true;
}

void glTFWindow::ShowWindow()
{
    GLFWwindow* window = glfwCreateWindow(800, 600, "Hello World", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return;
    }

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }

    glfwTerminate();
}
