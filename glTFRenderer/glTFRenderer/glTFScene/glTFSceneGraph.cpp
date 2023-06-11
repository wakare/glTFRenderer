#include "glTFSceneGraph.h"

glTFSceneGraph::glTFSceneGraph()
    : m_root(std::make_unique<glTFSceneNode>())
{
    
}

void glTFSceneGraph::AddSceneNode(std::unique_ptr<glTFSceneNode>&& node)
{
    assert(node.get());
    m_root->m_children.push_back(std::move(node));
}

void glTFSceneGraph::Tick(size_t deltaTimeMs)
{
    TraverseNodesInner([](glTFSceneNode& node)
    {
        for (const auto& sceneObject : node.m_objects)
        {
            if (sceneObject->CanTick())
            {
                sceneObject->Tick();
            }    
        }
        
        return true;
    });
}

bool TraverseNodeImpl(const std::function<bool(glTFSceneNode&)>& visitor, glTFSceneNode& nodeToVisit)
{
    bool needVisitorNext = visitor(nodeToVisit);
    if (needVisitorNext && !nodeToVisit.m_children.empty())
    {
        for (const auto& child : nodeToVisit.m_children)
        {
            needVisitorNext = TraverseNodeImpl(visitor, *child);
            if (!needVisitorNext)
            {
                return false;
            }
        }
    }

    return needVisitorNext;
}

void glTFSceneGraph::TraverseNodes(const std::function<bool(const glTFSceneNode&)>& visitor) const
{
    TraverseNodeImpl(visitor, *m_root);
}

std::vector<glTFCamera*> glTFSceneGraph::GetSceneCameras() const
{
    std::vector<glTFCamera*> cameras;
    TraverseNodes([&cameras](const glTFSceneNode& node)
    {
        for (const auto& sceneObject : node.m_objects)
        {
            if (auto* camera = dynamic_cast<glTFCamera*>(sceneObject.get()) )
            {
                cameras.push_back(camera);
            }    
        }
            
        return true;
    });

    return cameras;
}

void glTFSceneGraph::TraverseNodesInner(const std::function<bool(glTFSceneNode&)>& visitor) const
{
    TraverseNodeImpl(visitor, *m_root);
}
