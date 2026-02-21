#include "RenderWindow/RendererInputDevice.h"
#include <GLFW/glfw3.h>

RendererInputDevice::RendererInputDevice()
    : m_cursor_offset({})
    , m_cursor({0.0f})
{
    m_key_state_pressed.resize(static_cast<size_t>(InputDeviceKeyType::KEY_COUNT));
    m_mouse_button_state_pressed.resize(static_cast<size_t>(GLFW_MOUSE_BUTTON_LAST + 1));
}

size_t ToInputDeviceKeyIndex(InputDeviceKeyType key)
{
    return static_cast<size_t>(key);
}

bool IsValidInputDeviceKey(InputDeviceKeyType key)
{
    const size_t key_index = ToInputDeviceKeyIndex(key);
    return key != InputDeviceKeyType::KEY_UNKNOWN &&
        key_index < static_cast<size_t>(InputDeviceKeyType::KEY_COUNT);
}

int ToGLFWButton(InputDeviceButtonType button)
{
    switch (button) {
    case InputDeviceButtonType::MOUSE_BUTTON_LEFT:
        return GLFW_MOUSE_BUTTON_LEFT;
        break;
    case InputDeviceButtonType::MOUSE_BUTTON_RIGHT:
        return GLFW_MOUSE_BUTTON_RIGHT;
        break;
    case InputDeviceButtonType::MOUSE_BUTTON_MIDDLE:
        return GLFW_MOUSE_BUTTON_MIDDLE;
        break;
    }
    return -1;
}

void RendererInputDevice::RecordKeyPressed(InputDeviceKeyType key_code)
{
    if (!IsValidInputDeviceKey(key_code))
    {
        return;
    }

    m_key_state_pressed[ToInputDeviceKeyIndex(key_code)] = true;
}

void RendererInputDevice::RecordKeyRelease(InputDeviceKeyType key_code)
{
    if (!IsValidInputDeviceKey(key_code))
    {
        return;
    }

    m_key_state_pressed[ToInputDeviceKeyIndex(key_code)] = false;
}

void RendererInputDevice::RecordMouseButtonPressed(InputDeviceButtonType button_code)
{
    const int button_index = ToGLFWButton(button_code);
    if (button_index < 0 || button_index >= static_cast<int>(m_mouse_button_state_pressed.size()))
    {
        return;
    }

    m_mouse_button_state_pressed[button_index] = true;
}

void RendererInputDevice::RecordMouseButtonRelease(InputDeviceButtonType button_code)
{
    const int button_index = ToGLFWButton(button_code);
    if (button_index < 0 || button_index >= static_cast<int>(m_mouse_button_state_pressed.size()))
    {
        return;
    }

    m_mouse_button_state_pressed[button_index] = false;
}

bool RendererInputDevice::IsKeyPressed(InputDeviceKeyType key_code) const
{
    if (!IsValidInputDeviceKey(key_code))
    {
        return false;
    }

    return m_key_state_pressed[ToInputDeviceKeyIndex(key_code)];
}

bool RendererInputDevice::IsMouseButtonPressed(InputDeviceButtonType button_code) const
{
    const int button_index = ToGLFWButton(button_code);
    if (button_index < 0 || button_index >= static_cast<int>(m_mouse_button_state_pressed.size()))
    {
        return false;
    }

    return m_mouse_button_state_pressed[button_index];
}

void RendererInputDevice::TickFrame(size_t delta_time_ms)
{
    ResetCursorOffset();
}

CursorPosition RendererInputDevice::GetCursorOffset() const
{
    return m_cursor_offset;
}

void RendererInputDevice::ResetCursorOffset()
{
    m_cursor_offset = {0.0, 0.0};
}

void RendererInputDevice::RecordCursorPos(double X, double Y)
{
    m_cursor_offset = {m_cursor.X - X, m_cursor.Y - Y};
    m_cursor = {X, Y};
}
