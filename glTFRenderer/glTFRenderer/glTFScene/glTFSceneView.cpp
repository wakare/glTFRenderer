#include "glTFSceneView.h"

glTFSceneView::glTFSceneView(const glTFSceneGraph& graph)
    : m_sceneGraph(graph)
{
}

void glTFSceneView::TraverseSceneObjectWithinView(const std::function<bool(const glTFSceneNode& primitive)>& visitor)
{
    // TODO: Do culling?
    if (m_cameras.empty())
    {
        m_cameras = m_sceneGraph.GetSceneCameras();    
    }
    
    m_sceneGraph.TraverseNodes(visitor);
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
