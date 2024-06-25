#pragma once
#include <GLFW/glfw3.h>

struct CursorPosition
{
    double X;
    double Y;
};

class glTFInputManager
{
public:
    glTFInputManager();
    
    void RecordKeyPressed(int key_code);
    void RecordKeyRelease(int key_code);
    
    void RecordMouseButtonPressed(int button_code);
    void RecordMouseButtonRelease(int button_code);
    
    void RecordCursorPos(double X, double Y);
    
    bool IsKeyPressed(int key_code) const;
    bool IsMouseButtonPressed(int mouse_button) const;

    void TickFrame(size_t delta_time_ms);
    
    CursorPosition GetCursorOffset() const;
    
private:
    void ResetCursorOffset();
    
    bool m_key_state_pressed[GLFW_KEY_LAST] = {false};
    bool m_mouse_button_state_pressed[GLFW_MOUSE_BUTTON_MIDDLE + 1] = {false};
    
    CursorPosition m_cursor_offset;
    CursorPosition m_cursor;
};
