#include "glTFLoader/glTFLoader.h"

#include "glTFWindow/glTFWindow.h"

bool ShowGLFWWindow()
{
    glTFWindow window;
    if (!window.InitAndShowWindow())
    {
        return false;
    }

    window.UpdateWindow();
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