#include "glTFLoader/glTFLoader.h"
#include "glTFWindow/glTFWindow.h"

bool ShowGLFWWindow()
{
    if (!glTFWindow::Get().InitAndShowWindow())
    {
        return false;
    }

    glTFWindow::Get().UpdateWindow();
    return true;
}

int main(int argc, char* argv[])
{
    /*
    glTFLoader loader;
    const bool loaded = loader.LoadFile("glTFSamples/Hello_glTF.json");
    if (loaded)
    {
        loader.Print();    
    }
    */

    if (ShowGLFWWindow())
    {
        return EXIT_FAILURE;
    }
    
    return 0;
}