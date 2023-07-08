#include "glTFSceneView.h"

#include "../glTFUtils/glTFLog.h"
#include "../glTFWindow/glTFInputManager.h"

glTFSceneView::glTFSceneView(const glTFSceneGraph& graph)
    : m_scene_graph(graph)
{
}

void glTFSceneView::TraverseSceneObjectWithinView(const std::function<bool(const glTFSceneNode& primitive)>& visitor)
{
    // TODO: Do culling?
    if (m_cameras.empty())
    {
        m_cameras = m_scene_graph.GetSceneCameras();    
    }
    
    m_scene_graph.TraverseNodes(visitor);
}

glm::mat4 glTFSceneView::GetViewProjectionMatrix() const
{
    if (m_cameras.empty())
    {
        assert(false);
        return glm::mat4(1.0f);    
    }

    // Use first camera as main camera
    return m_cameras[0]->GetViewProjectionMatrix();
}

glm::mat4 glTFSceneView::GetViewMatrix() const
{
    if (m_cameras.empty())
    {
        assert(false);
        return glm::mat4(1.0f);    
    }

    return m_cameras[0]->GetViewMatrix();
}

glm::mat4 glTFSceneView::GetProjectionMatrix() const
{
    if (m_cameras.empty())
    {
        assert(false);
        return glm::mat4(1.0f);    
    }

    return m_cameras[0]->GetProjectionMatrix();
}

void glTFSceneView::ApplyInput(glTFInputManager& input_manager, size_t delta_time_ms) const
{
    // Manipulate one camera
    glTFCamera* main_camera = m_cameras.empty() ? nullptr : m_cameras[0];
    if (!main_camera)
    {
        return;
    }
    
    bool need_apply_movement = false;
    glm::fvec4 delta_position = {0.0f, 0.0f, 0.0f, 0.0f};
    glm::fvec3 delta_rotation = glm::fvec3(0.0f);
    
    if (main_camera->GetCameraMode() == CameraMode::Free)
    {
        // Handle movement
        if (input_manager.IsKeyPressed(GLFW_KEY_W))
        {
            delta_position.z += 1.0f;
            need_apply_movement = true;
        }
    
        if (input_manager.IsKeyPressed(GLFW_KEY_S))
        {
            delta_position.z -= 1.0f;
            need_apply_movement = true;
        }
    
        if (input_manager.IsKeyPressed(GLFW_KEY_A))
        {
            delta_position.x += 1.0f;
            need_apply_movement = true;
        }
    
        if (input_manager.IsKeyPressed(GLFW_KEY_D))
        {
            delta_position.x -= 1.0f;
            need_apply_movement = true;
        }
    
        if (input_manager.IsKeyPressed(GLFW_KEY_Q))
        {
            delta_position.y += 1.0f;
            need_apply_movement = true;
        }
    
        if (input_manager.IsKeyPressed(GLFW_KEY_E))
        {
            delta_position.y -= 1.0f;
            need_apply_movement = true;
        }
    }
    else
    {
        if (input_manager.IsKeyPressed(GLFW_KEY_W))
        {
            delta_rotation.x += 1.0f;
            need_apply_movement = true;
        }
    
        if (input_manager.IsKeyPressed(GLFW_KEY_S))
        {
            delta_rotation.x -= 1.0f;
            need_apply_movement = true;
        }
    
        if (input_manager.IsKeyPressed(GLFW_KEY_A))
        {
            delta_rotation.y += 1.0f;
            need_apply_movement = true;
        }
    
        if (input_manager.IsKeyPressed(GLFW_KEY_D))
        {
            delta_rotation.y -= 1.0f;
            need_apply_movement = true;
        }
    }
    
    if (input_manager.IsKeyPressed(GLFW_KEY_LEFT_CONTROL) ||
        input_manager.IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT))
    {
        const glm::vec2 cursor_offset = input_manager.GetCursorOffset();
        input_manager.ResetCursorOffset();
        delta_rotation.y += cursor_offset.x;
        delta_rotation.x += cursor_offset.y;
        if (fabs(delta_rotation.x) > 0.0f || fabs(delta_rotation.y) > 0.0f)
        {
            need_apply_movement = true;
        }
    }

    if (!need_apply_movement)
    {
        return;
    }
    
    // Apply movement to scene view
    const float translation_scale = static_cast<float>(delta_time_ms) / 1000.0f;
    const float rotation_scale = static_cast<float>(delta_time_ms) / 1000.0f;
    
    delta_position *= translation_scale;
    delta_position.w = 1.0f;
    delta_rotation *= rotation_scale;

    if (main_camera->GetCameraMode() == CameraMode::Free)
    {
        main_camera->Translate(delta_position);
        main_camera->Rotate(delta_rotation);    
    }
    else if (main_camera->GetCameraMode() == CameraMode::Observer)
    {
        main_camera->ObserveRotateXY(delta_rotation.x, delta_rotation.y);
    }
    
    main_camera->MarkDirty();
}
