#pragma once
#include <vector>

struct CursorPosition
{
    double X;
    double Y;
};

enum class InputDeviceKeyType
{
    KEY_UNKNOWN,
    KEY_W,
    KEY_S,
    KEY_A,
    KEY_D,
    KEY_O,
    KEY_F,
    KEY_Q,
    KEY_E,
    KEY_R,
    KEY_C,
    KEY_V,
    KEY_ENTER,
    KEY_SPACE,
    KEY_LEFT_CONTROL,
    KEY_RIGHT_CONTROL,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
};

enum class InputDeviceButtonType
{
    BUTTON_UNKNOWN,
    MOUSE_BUTTON_LEFT,
    MOUSE_BUTTON_RIGHT,
    MOUSE_BUTTON_MIDDLE,
};

class RendererInputDevice
{
public:
    RendererInputDevice();
    
    void RecordKeyPressed(InputDeviceKeyType key_code);
    void RecordKeyRelease(InputDeviceKeyType key_code);
    
    void RecordMouseButtonPressed(InputDeviceButtonType button_code);
    void RecordMouseButtonRelease(InputDeviceButtonType button_code);
    
    void RecordCursorPos(double X, double Y);
    
    bool IsKeyPressed(InputDeviceKeyType key_code) const;
    bool IsMouseButtonPressed(InputDeviceButtonType mouse_button) const;

    void TickFrame(size_t delta_time_ms);
    
    CursorPosition GetCursorOffset() const;
    
private:
    void ResetCursorOffset();
    
    std::vector<bool> m_key_state_pressed;
    std::vector<bool> m_mouse_button_state_pressed;
    
    CursorPosition m_cursor_offset;
    CursorPosition m_cursor;
};
