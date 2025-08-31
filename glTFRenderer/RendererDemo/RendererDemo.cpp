#include "RendererInterface.h"

int main(int argc, char* argv[])
{
    RendererInterface::RenderWindowDesc window_desc{};
    window_desc.width = 1280;
    window_desc.height = 720;
    RendererInterface::RenderWindow window(window_desc);
    
    RendererInterface::RenderDeviceDesc device{};
    device.window = window.GetHandle();
    device.type = RendererInterface::DX12;
    
    RendererInterface::ResourceAllocator allocator(device);

    window.TickWindow();
    
    return 0;
}
