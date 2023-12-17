#pragma once
#include <glm/glm/glm.hpp>
#include <GLFW/glfw3.h>

#include "glTFApp/glTFAppRenderPipeline.h"

class glTFSceneView;

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
    
    void TickSceneView(glTFSceneView& view, size_t delta_time_ms);
    void TickRenderPipeline(glTFAppRenderPipelineBase& View, size_t delta_time_ms);
    
    glm::fvec2 GetCursorOffsetAndReset();
    
private:
    void ResetCursorOffset();
    
    bool m_key_state_pressed[GLFW_KEY_LAST] = {false};
    bool m_mouse_button_state_pressed[GLFW_MOUSE_BUTTON_MIDDLE + 1] = {false};
    
    glm::fvec2 m_cursor_offset;
    glm::fvec2 m_cursor;
};
