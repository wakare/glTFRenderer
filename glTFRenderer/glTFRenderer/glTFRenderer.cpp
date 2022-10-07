#include "glTFLoader/glTFLoader.h"

#include <GLFW/glfw3.h>

bool ShowGLFWWindow()
{
    if (!glfwInit())
    {
        return false;
    }

    GLFWwindow* window = glfwCreateWindow(800, 600, "Hello World", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return false;
    }

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }

    glfwTerminate();
    
    return true;
}

int main(int argc, char* argv[])
{
    glTFLoader loader;
    const bool loaded = loader.LoadFile("glTFSamples/Hello_glTF.json");
    if (loaded)
    {
        loader.Print();    
    }

    if (ShowGLFWWindow())
    {
        return EXIT_FAILURE;
    }
    
    return 0;
}