#include "glTFSceneGraph.h"

glTFSceneGraph::glTFSceneGraph()
    : m_root(std::make_unique<glTFSceneNode>())
{
    
}

void glTFSceneGraph::AddSceneNode(std::unique_ptr<glTFSceneNode>&& node)
{
    assert(node.get());
    m_root->children.push_back(std::move(node));
}

void glTFSceneGraph::Tick()
{
    TraverseNodesInner([](glTFSceneNode& node)
    {
        if (node.object && node.object->CanTick())
        {
            node.object->Tick();
        }
        
        return true;
    });
}

bool TraverseNodeImpl(const std::function<bool(glTFSceneNode&)>& visitor, glTFSceneNode& nodeToVisit)
{
    bool needVisitorNext = visitor(nodeToVisit);
    if (needVisitorNext && !nodeToVisit.children.empty())
    {
        for (const auto& child : nodeToVisit.children)
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

std::vector<const glTFCamera*> glTFSceneGraph::GetSceneCameras() const
{
    std::vector<const glTFCamera*> cameras;
    TraverseNodes([&cameras](const glTFSceneNode& node)
    {
        if (const auto* camera = dynamic_cast<glTFCamera*>(node.object.get()) )
        {
            cameras.push_back(camera);
        }
            
        return true;
    });

    return cameras;
}

void glTFSceneGraph::TraverseNodesInner(const std::function<bool(glTFSceneNode&)>& visitor) const
{
    TraverseNodeImpl(visitor, *m_root);
}
