#include "glTFSceneView.h"

#include <gtx/norm.hpp>

#include "RenderWindow/glTFInputManager.h"
#include "glTFRenderPass/glTFRenderPassManager.h"
#include "glTFRenderPass/glTFComputePass/glTFComputePassLighting.h"

glTFSceneView::glTFSceneView(const glTFSceneGraph& graph)
    : m_scene_graph(graph)
    , m_lighting_dirty(true)
{

}

void glTFSceneView::TraverseSceneObjectWithinView(const std::function<bool(const glTFSceneNode& primitive)>& visitor) const
{
    m_scene_graph.TraverseNodes(visitor);
}

std::vector<const glTFSceneNode*> glTFSceneView::GetDirtySceneNodes() const
{
    return m_frame_dirty_nodes;
}

glm::mat4 glTFSceneView::GetViewProjectionMatrix() const
{
    return GetMainCamera() ? GetMainCamera()->GetViewProjectionMatrix() : glm::mat4(1.0f);
}

glm::mat4 glTFSceneView::GetViewMatrix() const
{
    return GetMainCamera() ? GetMainCamera()->GetViewMatrix() : glm::mat4(1.0f);
}

glm::mat4 glTFSceneView::GetProjectionMatrix() const
{
    return GetMainCamera() ? GetMainCamera()->GetProjectionMatrix() : glm::mat4(1.0f);
}

void glTFSceneView::ApplyInput(const glTFInputManager& input_manager, size_t delta_time_ms) const
{
    // Manipulate one camera
    glTFCamera* main_camera = GetMainCamera();
    if (!main_camera)
    {
        return;
    }

    // Focus scene center
    if (input_manager.IsKeyPressed(GLFW_KEY_O))
    {
        main_camera->SetCameraMode(CameraMode::Observer);
        FocusSceneCenter(*main_camera);    
    }
    if (input_manager.IsKeyPressed(GLFW_KEY_F))
    {
        main_camera->SetCameraMode(CameraMode::Free);
    }
    
    ApplyInputForCamera(input_manager, *main_camera, delta_time_ms);
}

void glTFSceneView::GetViewportSize(unsigned& out_width, unsigned& out_height) const
{
    GetMainCamera()->GetCameraViewportSize(out_width, out_height);
}

glTFCamera* glTFSceneView::GetMainCamera() const
{
    const auto cameras = m_scene_graph.GetSceneCameras();
    return cameras.empty() ? nullptr : cameras[0];
}

void glTFSceneView::Tick(const glTFSceneGraph& scene_graph)
{
    m_lighting_dirty = false;
    
    scene_graph.TraverseNodes([this](const glTFSceneNode& node)
    {
        for (const auto& object : node.m_objects)
        {
            if (node.IsDirty() && dynamic_cast<const glTFLightBase*>(object.get()))
            {
                m_lighting_dirty = true;
                return false;
            }
        }
        
        return true; 
    });
}

void glTFSceneView::GatherDirtySceneNodes()
{
    m_frame_dirty_nodes.clear();
    
    TraverseSceneObjectWithinView([this](const glTFSceneNode& node)
    {
        if (node.IsDirty())
        {
            m_frame_dirty_nodes.push_back(&node);
        }

        return true;
    });

    for (auto* node : m_frame_dirty_nodes)
    {
        node->ResetDirty();
    }
}


bool glTFSceneView::GetLightingDirty() const
{
    return m_lighting_dirty;
}

ConstantBufferSceneView glTFSceneView::CreateSceneViewConstantBuffer(glTFRenderResourceManager& resource_manager) const
{
    ConstantBufferSceneView scene_view;
    
    unsigned width, height;
    GetViewportSize(width, height);

    auto* camera = GetMainCamera();
    GLTF_CHECK(camera);
    
    scene_view.prev_view_matrix = scene_view.view_matrix;
    scene_view.prev_projection_matrix = scene_view.projection_matrix;
    
    scene_view.view_matrix = GetViewMatrix();
    scene_view.projection_matrix = GetProjectionMatrix();
    scene_view.inverse_view_matrix = glm::inverse(scene_view.view_matrix);
    scene_view.inverse_projection_matrix = glm::inverse(scene_view.projection_matrix);

    scene_view.view_position = {camera->GetCameraPosition(), 1.0f};
    scene_view.viewport_width = width;
    scene_view.viewport_height = height;

    scene_view.nearZ = camera->GetNearZPlane();
    scene_view.farZ = camera->GetFarZPlane();

    glm::mat4 rotate_right_plane = glm::rotate(glm::identity<glm::mat4>(), camera->GetFovX() * 0.5f, {0.0, 1.0, 0.0}); 
    scene_view.view_right_plane_normal = rotate_right_plane * glm::vec4{-1.0f, 0.0f, 0.0f, 0.0f};
    scene_view.view_left_plane_normal = scene_view.view_right_plane_normal;
    scene_view.view_left_plane_normal.x = -scene_view.view_left_plane_normal.x;

    glm::mat4 rotate_up_plane = glm::rotate(glm::identity<glm::mat4>(), camera->GetFovY() * 0.5f, {-1.0, 0.0, 0.0});
    scene_view.view_up_plane_normal = rotate_up_plane * glm::vec4{0.0f, -1.0f, 0.0f, 0.0f};
    scene_view.view_down_plane_normal = scene_view.view_up_plane_normal;
    scene_view.view_down_plane_normal.y = -scene_view.view_down_plane_normal.y;

    scene_view.view_left_plane_normal = glm::normalize(scene_view.view_left_plane_normal);
    scene_view.view_right_plane_normal = glm::normalize(scene_view.view_right_plane_normal);
    scene_view.view_up_plane_normal = glm::normalize(scene_view.view_up_plane_normal);
    scene_view.view_down_plane_normal = glm::normalize(scene_view.view_down_plane_normal);

    return scene_view;
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

void glTFSceneView::ApplyInputForCamera(const glTFInputManager& input_manager, glTFCamera& camera, size_t delta_time_ms)
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
            delta_translation.x -= 1.0f;
            need_apply_movement = true;
        }
    
        if (input_manager.IsKeyPressed(GLFW_KEY_D))
        {
            delta_translation.x += 1.0f;
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
        const auto cursor_offset = input_manager.GetCursorOffset();
        
        delta_rotation.y -= cursor_offset.X;
        delta_rotation.x -= cursor_offset.Y;
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
    
    // Convert view space translation to world space
    delta_translation = glm::inverse(camera.GetViewMatrix()) * glm::fvec4{delta_translation, 0.0f};
    delta_translation *= translation_scale;
    
    delta_rotation *= rotation_scale;

    if (camera.GetCameraMode() == CameraMode::Free)
    {
        if (glm::length2(delta_translation) > 0)
        {
            camera.TranslateOffset(delta_translation);    
        }

        if (glm::length2(delta_rotation) > 0)
        {
            camera.RotateOffset(delta_rotation);    
        }
    }
    else if (camera.GetCameraMode() == CameraMode::Observer)
    {
        camera.TranslateOffset(delta_translation);
        camera.ObserveRotateXY(delta_rotation.x, delta_rotation.y);
    }
    
    camera.MarkDirty();
}
