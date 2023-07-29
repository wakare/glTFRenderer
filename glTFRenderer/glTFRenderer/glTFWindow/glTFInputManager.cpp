#include "glTFInputManager.h"
#include <glm/glm/glm.hpp>

#include "../glTFScene/glTFSceneView.h"

glTFInputManager::glTFInputManager()
    : m_cursor_offset(0.0f)
    , m_cursor(0.0f)
{
    memset(m_key_state_pressed, 0, sizeof(m_key_state_pressed));
}

void glTFInputManager::RecordKeyPressed(int keyCode)
{
    m_key_state_pressed[keyCode] = true;
}

void glTFInputManager::RecordKeyRelease(int keyCode)
{
    m_key_state_pressed[keyCode] = false;
}

void glTFInputManager::RecordMouseButtonPressed(int buttonCode)
{
    m_mouse_button_state_pressed[buttonCode] = true;
}

void glTFInputManager::RecordMouseButtonRelease(int buttonCode)
{
    m_mouse_button_state_pressed[buttonCode] = false;
}

bool glTFInputManager::IsKeyPressed(int keyCode) const
{
    return m_key_state_pressed[keyCode];
}

bool glTFInputManager::IsMouseButtonPressed(int mouseButton) const
{
    return m_mouse_button_state_pressed[mouseButton];
}

void glTFInputManager::TickSceneView(glTFSceneView& View, size_t deltaTimeMs)
{
    View.ApplyInput(*this, deltaTimeMs);
}

glm::fvec2 glTFInputManager::GetCursorOffsetAndReset()
{
    const auto result = m_cursor_offset;
    ResetCursorOffset();
    return result;
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
