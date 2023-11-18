#include "glTFRenderInterfaceSceneView.h"

#include "glTFScene/glTFSceneView.h"

glTFRenderInterfaceSceneView::glTFRenderInterfaceSceneView()
    : m_scene_view({})
{
}

bool glTFRenderInterfaceSceneView::UpdateSceneView(const glTFSceneView& view)
{
    unsigned width, height;
    view.GetViewportSize(width, height);

    m_scene_view.prev_view_matrix = m_scene_view.view_matrix;
    m_scene_view.prev_projection_matrix = m_scene_view.projection_matrix;
    
    m_scene_view.view_matrix = view.GetViewMatrix();
    m_scene_view.projection_matrix = view.GetProjectionMatrix();
    m_scene_view.inverse_view_matrix = glm::inverse(m_scene_view.view_matrix);
    m_scene_view.inverse_projection_matrix = glm::inverse(m_scene_view.projection_matrix);

    m_scene_view.view_position = view.GetMainCameraWorldPosition();
    m_scene_view.viewport_width = width;
    m_scene_view.viewport_height = height;
            
    UploadCPUBuffer(&m_scene_view, 0, sizeof(m_scene_view));
    
    return true;
}
