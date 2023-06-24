#pragma once
#include <glm/glm/glm.hpp>
#include <GLFW/glfw3.h>

class glTFSceneView;

class glTFInputManager
{
public:
    glTFInputManager();
    
    void RecordKeyPressed(int keyCode);
    void RecordKeyRelease(int keyCode);
    
    void RecordMouseButtonPressed(int buttonCode);
    void RecordMouseButtonRelease(int buttonCode);
    
    void RecordCursorPos(double X, double Y);
    
    bool IsKeyPressed(int keyCode) const;
    bool IsMouseButtonPressed(int mouseButton) const;
    
    void TickSceneView(glTFSceneView& View, size_t deltaTimeMs);
    glm::fvec2 GetCursorOffset() const;
    void ResetCursorOffset();
    
private:
    bool m_keyStatePressed[GLFW_KEY_LAST] = {false};
    bool m_mouseButtonStatePressed[GLFW_MOUSE_BUTTON_MIDDLE + 1] = {false};
    
    glm::fvec2 m_cursorOffset;
    glm::fvec2 m_cursor;
};
