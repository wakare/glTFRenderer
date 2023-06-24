#include "glTFInputManager.h"
#include <glm/glm/glm.hpp>

#include "../glTFScene/glTFSceneView.h"

glTFInputManager::glTFInputManager()
    : m_cursorOffset(0.0f)
    , m_cursor(0.0f)
{
    memset(m_keyStatePressed, 0, sizeof(m_keyStatePressed));
}

void glTFInputManager::RecordKeyPressed(int keyCode)
{
    m_keyStatePressed[keyCode] = true;
}

void glTFInputManager::RecordKeyRelease(int keyCode)
{
    m_keyStatePressed[keyCode] = false;
}

void glTFInputManager::RecordMouseButtonPressed(int buttonCode)
{
    m_mouseButtonStatePressed[buttonCode] = true;
}

void glTFInputManager::RecordMouseButtonRelease(int buttonCode)
{
    m_mouseButtonStatePressed[buttonCode] = false;
}

bool glTFInputManager::IsKeyPressed(int keyCode) const
{
    return m_keyStatePressed[keyCode];
}

bool glTFInputManager::IsMouseButtonPressed(int mouseButton) const
{
    return m_mouseButtonStatePressed[mouseButton];
}

void glTFInputManager::TickSceneView(glTFSceneView& View, size_t deltaTimeMs)
{
    View.ApplyInput(*this, deltaTimeMs);
}

glm::fvec2 glTFInputManager::GetCursorOffset() const
{
    return m_cursorOffset;
}

void glTFInputManager::ResetCursorOffset()
{
    m_cursorOffset = {0.0f, 0.0f};
}

void glTFInputManager::RecordCursorPos(double X, double Y)
{
    m_cursorOffset = glm::vec2{m_cursor.x - X, m_cursor.y - Y};
    m_cursor = {X, Y};
}
