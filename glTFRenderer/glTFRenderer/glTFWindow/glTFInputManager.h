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
    glm::fvec2 GetCursorOffsetAndReset();
    
private:
    void ResetCursorOffset();
    
    bool m_key_state_pressed[GLFW_KEY_LAST] = {false};
    bool m_mouse_button_state_pressed[GLFW_MOUSE_BUTTON_MIDDLE + 1] = {false};
    
    glm::fvec2 m_cursor_offset;
    glm::fvec2 m_cursor;
};
