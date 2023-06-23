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

    void TickSceneView(glTFSceneView& View, size_t deltaTimeMs);
    
private:
    bool m_keyStatePressed[GLFW_KEY_LAST] = {false};
    bool m_mouseButtonStatePressed[GLFW_MOUSE_BUTTON_MIDDLE + 1] = {false};
    
    glm::fvec2 m_cursorOffset;
    glm::fvec2 m_cursor;
};
