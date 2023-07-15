#include "glTFSceneView.h"

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
    glTFCamera* main_camera = GetMainCamera();
    if (!main_camera)
    {
        return;
    }

    // Focus scene center
    if (input_manager.IsKeyPressed(GLFW_KEY_F))
    {
        FocusSceneCenter(*main_camera);
    }
    
    ApplyInputForCamera(input_manager, *main_camera, delta_time_ms);
}

glTFCamera* glTFSceneView::GetMainCamera() const
{
    return  m_cameras.empty() ? nullptr : m_cameras[0];
}

void glTFSceneView::FocusSceneCenter(glTFCamera& camera) const
{
    glTF_AABB::AABB sceneAABB;
    m_scene_graph.TraverseNodes([&sceneAABB](const glTFSceneNode& node)
    {
        for (const auto& object : node.m_objects)
        {
            const glTF_AABB::AABB world_AABB = glTF_AABB::AABB::TransformAABB(node.m_finalTransform.GetTransformMatrix(), object->GetAABB());
            sceneAABB.extend(world_AABB);
        }
        
        return true;
    });

    camera.Translate(sceneAABB.getCenter() - glm::vec3 {0.0f, 0.0f, 2 * sceneAABB.getLongestEdge()});
    camera.Observe(sceneAABB.getCenter());
}

void glTFSceneView::ApplyInputForCamera(glTFInputManager& input_manager, glTFCamera& camera, size_t delta_time_ms) const
{
    bool need_apply_movement = false;
    glm::fvec3 delta_translation = {0.0f, 0.0f, 0.0f};
    glm::fvec3 delta_rotation = {0.0f, 0.0f, 0.0f};
    
    if (camera.GetCameraMode() == CameraMode::Free)
    {
        // Handle movement
        if (input_manager.IsKeyPressed(GLFW_KEY_W))
        {
            delta_translation.z += 1.0f;
            need_apply_movement = true;
        }
    
        if (input_manager.IsKeyPressed(GLFW_KEY_S))
        {
            delta_translation.z -= 1.0f;
            need_apply_movement = true;
        }
    
        if (input_manager.IsKeyPressed(GLFW_KEY_A))
        {
            delta_translation.x += 1.0f;
            need_apply_movement = true;
        }
    
        if (input_manager.IsKeyPressed(GLFW_KEY_D))
        {
            delta_translation.x -= 1.0f;
            need_apply_movement = true;
        }
    
        if (input_manager.IsKeyPressed(GLFW_KEY_Q))
        {
            delta_translation.y += 1.0f;
            need_apply_movement = true;
        }
    
        if (input_manager.IsKeyPressed(GLFW_KEY_E))
        {
            delta_translation.y -= 1.0f;
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

        if (input_manager.IsKeyPressed(GLFW_KEY_Q))
        {
            delta_translation.z += (camera.GetObserveDistance() + 1.0f);
            need_apply_movement = true;
        }
        
        if (input_manager.IsKeyPressed(GLFW_KEY_E))
        {
            delta_translation.z -= (camera.GetObserveDistance() + 1.0f);
            need_apply_movement = true;
        }
    }
    
    if (input_manager.IsKeyPressed(GLFW_KEY_LEFT_CONTROL) ||
        input_manager.IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT))
    {
        const glm::vec2 cursor_offset = input_manager.GetCursorOffset();
        input_manager.ResetCursorOffset();
        delta_rotation.y -= cursor_offset.x;
        delta_rotation.x -= cursor_offset.y;
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
    
    delta_translation *= translation_scale;
    // Convert view space translation to world space
    delta_translation = glm::inverse(camera.GetViewMatrix()) * glm::fvec4{delta_translation, 0.0f};
    
    delta_rotation *= rotation_scale;

    if (camera.GetCameraMode() == CameraMode::Free)
    {
        camera.TranslateOffset(delta_translation);
        camera.RotateOffset(delta_rotation);    
    }
    else if (camera.GetCameraMode() == CameraMode::Observer)
    {
        camera.TranslateOffset(delta_translation);
        camera.ObserveRotateXY(delta_rotation.x, delta_rotation.y);
    }
    
    camera.MarkDirty();
}
