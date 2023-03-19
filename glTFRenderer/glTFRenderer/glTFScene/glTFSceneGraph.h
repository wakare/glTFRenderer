#pragma once
#include <functional>
#include <memory>
#include <vector>

#include "glTFSceneObjectBase.h"

struct glTFSceneNode
{
    std::unique_ptr<glTFSceneObjectBase> object;
    std::vector<std::unique_ptr<glTFSceneNode>> children;
};

// Graph structure to manage scene objects
class glTFSceneGraph
{
public:
    glTFSceneGraph();
    void AddSceneNode(std::unique_ptr<glTFSceneNode>&& node);

    // visitor return value indicate whether continue visit remained nodes 
    void TraverseNodes(const std::function<bool(const glTFSceneNode&)>& visitor) const;
    
private:
    std::unique_ptr<glTFSceneNode> m_root;
};
