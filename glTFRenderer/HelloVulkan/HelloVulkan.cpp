#include "RenderWindow/glTFWindow.h"
#include "VulkanEngine.h"

int main(int argc, char* argv[])
{
    auto& window = glTFWindow::Get();
    VulkanEngine engine;
    
    window.InitAndShowWindow();
    {    
        engine.Init();
        window.SetTickCallback([&]()
        {
            engine.Update();
        });
        window.SetExitCallback([&]()
        {
            engine.UnInit();
        });
    }
    window.UpdateWindow();
    
    return 0;
}
