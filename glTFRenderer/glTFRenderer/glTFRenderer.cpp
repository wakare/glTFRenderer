#include "glTFLoader/glTFLoader.h"

int main(int argc, char* argv[])
{
    glTFLoader loader;
    const bool loaded = loader.LoadFile("glTFSamples/Hello_glTF.json");
    if (loaded)
    {
        loader.Print();    
    }
    
    return 0;
}
