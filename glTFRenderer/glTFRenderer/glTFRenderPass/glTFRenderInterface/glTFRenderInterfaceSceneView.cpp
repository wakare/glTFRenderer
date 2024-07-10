#include "glTFRenderInterfaceSceneView.h"

#include "glTFScene/glTFSceneView.h"

glTFRenderInterfaceSceneView::glTFRenderInterfaceSceneView()
    : m_scene_view({})
{
}

bool glTFRenderInterfaceSceneView::UpdateSceneView(glTFRenderResourceManager& resource_manager, const glTFSceneView& view)
{
    unsigned width, height;
    view.GetViewportSize(width, height);

    auto* camera = view.GetMainCamera();
    GLTF_CHECK(camera);
    
    m_scene_view.prev_view_matrix = m_scene_view.view_matrix;
    m_scene_view.prev_projection_matrix = m_scene_view.projection_matrix;
    
    m_scene_view.view_matrix = view.GetViewMatrix();
    m_scene_view.projection_matrix = view.GetProjectionMatrix();
    m_scene_view.inverse_view_matrix = glm::inverse(m_scene_view.view_matrix);
    m_scene_view.inverse_projection_matrix = glm::inverse(m_scene_view.projection_matrix);

    m_scene_view.view_position = {camera->GetCameraPosition(), 1.0f};
    m_scene_view.viewport_width = width;
    m_scene_view.viewport_height = height;

    m_scene_view.nearZ = camera->GetNearZPlane();
    m_scene_view.farZ = camera->GetFarZPlane();

    glm::mat4 rotate_right_plane = glm::rotate(glm::identity<glm::mat4>(), camera->GetFovX() * 0.5f, {0.0, 1.0, 0.0}); 
    m_scene_view.view_right_plane_normal = rotate_right_plane * glm::vec4{-1.0f, 0.0f, 0.0f, 0.0f};
    m_scene_view.view_left_plane_normal = m_scene_view.view_right_plane_normal;
    m_scene_view.view_left_plane_normal.x = -m_scene_view.view_left_plane_normal.x;

    glm::mat4 rotate_up_plane = glm::rotate(glm::identity<glm::mat4>(), camera->GetFovY() * 0.5f, {-1.0, 0.0, 0.0});
    m_scene_view.view_up_plane_normal = rotate_up_plane * glm::vec4{0.0f, -1.0f, 0.0f, 0.0f};
    m_scene_view.view_down_plane_normal = m_scene_view.view_up_plane_normal;
    m_scene_view.view_down_plane_normal.y = -m_scene_view.view_down_plane_normal.y;

    m_scene_view.view_left_plane_normal = glm::normalize(m_scene_view.view_left_plane_normal);
    m_scene_view.view_right_plane_normal = glm::normalize(m_scene_view.view_right_plane_normal);
    m_scene_view.view_up_plane_normal = glm::normalize(m_scene_view.view_up_plane_normal);
    m_scene_view.view_down_plane_normal = glm::normalize(m_scene_view.view_down_plane_normal);
    
    UploadCPUBuffer(resource_manager, &m_scene_view, 0, sizeof(m_scene_view));
    
    return true;
}
