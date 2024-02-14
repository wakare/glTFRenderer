#pragma once
#include <memory>

class IRHIDescriptorHeap;
class glTFRenderResourceManager;
class glTFWindow;

class glTFGUI
{
public:
    bool SetupGUIContext(const glTFWindow& window, glTFRenderResourceManager& resource_manager);
    bool RenderWidgets(glTFRenderResourceManager& resource_manager);
    bool SetupWidgetBegin();
    bool SetupWidgetEnd();
    bool ExitAndClean();

    static bool IsValid();
    
    static bool HandleMouseEventThisFrame();
    static bool HandleKeyBoardEventThisFrame();
    
private:
    static bool SetupWidgets();

    static bool g_valid;
    
    std::shared_ptr<IRHIDescriptorHeap> m_descriptor_heap;
};
