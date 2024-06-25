#include "glTFInputManager.h"
#include <glm/glm.hpp>

glTFInputManager::glTFInputManager()
    : m_cursor_offset(0.0f)
    , m_cursor(0.0f)
{
    memset(m_key_state_pressed, 0, sizeof(m_key_state_pressed));
}

void glTFInputManager::RecordKeyPressed(int key_code)
{
    m_key_state_pressed[key_code] = true;
}

void glTFInputManager::RecordKeyRelease(int key_code)
{
    m_key_state_pressed[key_code] = false;
}

void glTFInputManager::RecordMouseButtonPressed(int button_code)
{
    m_mouse_button_state_pressed[button_code] = true;
}

void glTFInputManager::RecordMouseButtonRelease(int button_code)
{
    m_mouse_button_state_pressed[button_code] = false;
}

bool glTFInputManager::IsKeyPressed(int key_code) const
{
    return m_key_state_pressed[key_code];
}

bool glTFInputManager::IsMouseButtonPressed(int mouse_button) const
{
    return m_mouse_button_state_pressed[mouse_button];
}

void glTFInputManager::TickFrame(size_t delta_time_ms)
{
    ResetCursorOffset();
}

glm::fvec2 glTFInputManager::GetCursorOffset() const
{
    return m_cursor_offset;
}

void glTFInputManager::ResetCursorOffset()
{
    m_cursor_offset = {0.0f, 0.0f};
}

void glTFInputManager::RecordCursorPos(double X, double Y)
{
    m_cursor_offset = glm::vec2{m_cursor.x - X, m_cursor.y - Y};
    m_cursor = {X, Y};
}
