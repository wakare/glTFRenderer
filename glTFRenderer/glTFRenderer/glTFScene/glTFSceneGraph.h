#pragma once
#include <functional>
#include <memory>
#include <vector>

#include "glTFSceneObjectBase.h"
#include "glTFCamera.h"
#include "../glTFMaterial/glTFMaterialBase.h"

struct glTFSceneNode
{
    mutable bool renderStateDirty = true;
    
    std::unique_ptr<glTFSceneObjectBase> object;
    std::vector<std::unique_ptr<glTFSceneNode>> children;
};

// Graph structure to manage scene objects
class glTFSceneGraph
{
public:
    glTFSceneGraph();
    void AddSceneNode(std::unique_ptr<glTFSceneNode>&& node);

    void Tick(size_t deltaTimeMs);
    
    // visitor return value indicate whether continue visit remained nodes 
    void TraverseNodes(const std::function<bool(const glTFSceneNode&)>& visitor) const;

    std::vector<glTFCamera*> GetSceneCameras() const;
    
protected:
    void TraverseNodesInner(const std::function<bool(glTFSceneNode&)>& visitor) const;
    
    std::unique_ptr<glTFSceneNode> m_root; 
};
