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
    if (ShowGLFWWindow())
    {
        return EXIT_FAILURE;
    }
    
    return 0;
}