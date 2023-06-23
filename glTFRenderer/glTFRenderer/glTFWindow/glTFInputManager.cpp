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

void glTFInputManager::TickSceneView(glTFSceneView& View, size_t deltaTimeMs)
{
    bool needApplyMovement = false;
    
    // Handle movement
    glm::fvec4 deltaPosition = {0.0f, 0.0f, 0.0f, 0.0f};
    if (m_keyStatePressed[GLFW_KEY_W])
    {
        deltaPosition.z += 1.0f;
        needApplyMovement = true;
    }
    
    if (m_keyStatePressed[GLFW_KEY_S])
    {
        deltaPosition.z -= 1.0f;
        needApplyMovement = true;
    }
    
    if (m_keyStatePressed[GLFW_KEY_A])
    {
        deltaPosition.x += 1.0f;
        needApplyMovement = true;
    }
    
    if (m_keyStatePressed[GLFW_KEY_D])
    {
        deltaPosition.x -= 1.0f;
        needApplyMovement = true;
    }
    
    if (m_keyStatePressed[GLFW_KEY_Q])
    {
        deltaPosition.y += 1.0f;
        needApplyMovement = true;
    }
    
    if (m_keyStatePressed[GLFW_KEY_E])
    {
        deltaPosition.y -= 1.0f;
        needApplyMovement = true;
    }
    
    // Handle rotation
    glm::fvec3 deltaRotation = glm::fvec3(0.0f);
    if (m_keyStatePressed[GLFW_KEY_LEFT_CONTROL] ||
        m_mouseButtonStatePressed[GLFW_MOUSE_BUTTON_LEFT])
    {
        deltaRotation.y += m_cursorOffset.x;
        deltaRotation.x += m_cursorOffset.y;
        m_cursorOffset = {0.0f, 0.0f};
        needApplyMovement = true;
    }

    if (!needApplyMovement)
    {
        return;
    }
    
    // Apply movement to scene view
    const float translationScale = static_cast<float>(deltaTimeMs) / 1000.0f;
    const float rotationScale = static_cast<float>(deltaTimeMs) / 1000.0f;
    
    deltaPosition *= translationScale;
    deltaRotation *= rotationScale;

    View.ApplyMovement(deltaPosition, deltaRotation);
}

void glTFInputManager::RecordCursorPos(double X, double Y)
{
    m_cursorOffset += glm::vec2{m_cursor.x - X, m_cursor.y - Y};
    m_cursor = {X, Y};
}
