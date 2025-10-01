#include "RenderWindow/RendererInputDevice.h"
#include <GLFW/glfw3.h>

RendererInputDevice::RendererInputDevice()
    : m_cursor_offset({})
    , m_cursor({0.0f})
{
    m_key_state_pressed.resize(GLFW_KEY_LAST);
    m_mouse_button_state_pressed.resize(GLFW_MOUSE_BUTTON_LAST);
}

unsigned ToGLFWKey(InputDeviceKeyType key)
{
    switch (key) {
    case InputDeviceKeyType::KEY_W:
        return GLFW_KEY_W;
        break;
    case InputDeviceKeyType::KEY_S:
        return GLFW_KEY_S;
        break;
    case InputDeviceKeyType::KEY_A:
        return GLFW_KEY_A;
        break;
    case InputDeviceKeyType::KEY_D:
        return GLFW_KEY_D;
        break;
    case InputDeviceKeyType::KEY_O:
        return GLFW_KEY_O;
        break;
    case InputDeviceKeyType::KEY_F:
        return GLFW_KEY_F;
        break;
    case InputDeviceKeyType::KEY_Q:
        return GLFW_KEY_Q;
        break;
    case InputDeviceKeyType::KEY_E:
        return GLFW_KEY_E;
        break;
    case InputDeviceKeyType::KEY_R:
        return GLFW_KEY_R;
        break;
    case InputDeviceKeyType::KEY_C:
        return GLFW_KEY_C;
        break;
    case InputDeviceKeyType::KEY_V:
        return GLFW_KEY_V;
        break;
    case InputDeviceKeyType::KEY_UP:
        return GLFW_KEY_UP;
        break;
    case InputDeviceKeyType::KEY_DOWN:
        return GLFW_KEY_DOWN;
        break;
    case InputDeviceKeyType::KEY_LEFT:
        return GLFW_KEY_LEFT;
        break;
    case InputDeviceKeyType::KEY_RIGHT:
        return GLFW_KEY_RIGHT;
        break;
    case InputDeviceKeyType::KEY_ENTER:
        return GLFW_KEY_ENTER;
        break;
    case InputDeviceKeyType::KEY_SPACE:
        return GLFW_KEY_SPACE;
        break;
    case InputDeviceKeyType::KEY_LEFT_CONTROL:
        return GLFW_KEY_LEFT_CONTROL;
        break;
    case InputDeviceKeyType::KEY_RIGHT_CONTROL:
        return GLFW_KEY_RIGHT_CONTROL;
        break;
    }
    return GLFW_KEY_UNKNOWN;
}

unsigned ToGLFWButton(InputDeviceButtonType button)
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
    return GLFW_MOUSE_BUTTON_LAST;
}

void RendererInputDevice::RecordKeyPressed(InputDeviceKeyType key_code)
{
    m_key_state_pressed[ToGLFWKey(key_code)] = true;
}

void RendererInputDevice::RecordKeyRelease(InputDeviceKeyType key_code)
{
    m_key_state_pressed[ToGLFWKey(key_code)] = false;
}

void RendererInputDevice::RecordMouseButtonPressed(InputDeviceButtonType button_code)
{
    m_mouse_button_state_pressed[ToGLFWButton(button_code)] = true;
}

void RendererInputDevice::RecordMouseButtonRelease(InputDeviceButtonType button_code)
{
    m_mouse_button_state_pressed[ToGLFWButton(button_code)] = false;
}

bool RendererInputDevice::IsKeyPressed(InputDeviceKeyType key_code) const
{
    return m_key_state_pressed[ToGLFWKey(key_code)];
}

bool RendererInputDevice::IsMouseButtonPressed(InputDeviceButtonType button_code) const
{
    return m_mouse_button_state_pressed[ToGLFWButton(button_code)];
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
