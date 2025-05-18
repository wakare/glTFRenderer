#pragma once
#include <functional>

#include "RendererCommon.h"

class glTFRenderResourceManager;
class glTFWindow;

typedef std::function<void()> GUIWidgetSetupCallback;

class glTFGUIRenderer
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(glTFGUIRenderer)
    
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
