#pragma once
#include <functional>

class glTFRenderResourceManager;
class glTFWindow;

typedef std::function<void()> GUIWidgetSetupCallback;

class glTFGUIRenderer
{
public:
    ~glTFGUIRenderer();
    
    bool SetupGUIContext(const glTFWindow& window, glTFRenderResourceManager& resource_manager);
    bool RenderWidgets(glTFRenderResourceManager& resource_manager);
    bool UpdateWidgets();
    
    bool ExitAndClean();

    bool HandleMouseEventThisFrame() const;
    bool HandleKeyBoardEventThisFrame() const;
    
    bool AddWidgetSetupCallback(GUIWidgetSetupCallback callback);
    
private:
    bool SetupWidgetsInternal();
    
    bool m_inited {false};
    
    std::vector<GUIWidgetSetupCallback> m_widget_setup_callbacks;
};
