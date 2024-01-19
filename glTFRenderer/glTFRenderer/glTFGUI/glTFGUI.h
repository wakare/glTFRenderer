#pragma once
#include <memory>

class IRHIDescriptorHeap;
class glTFRenderResourceManager;
class glTFWindow;

class glTFGUI
{
public:
    bool SetupGUIContext(const glTFWindow& window, glTFRenderResourceManager& resource_manager);
    bool RenderNewFrame(glTFRenderResourceManager& resource_manager);
    bool ExitAndClean();

    static bool HandleMouseEventThisFrame();
    static bool HandleKeyBoardEventThisFrame();
    
private:
    std::shared_ptr<IRHIDescriptorHeap> m_descriptor_heap;
};
