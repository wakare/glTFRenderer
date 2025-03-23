#pragma once
#include <functional>
#include <memory>
#include <vector>

#include "glTFSceneObjectBase.h"
#include "glTFLoader/glTFLoader.h"
#include "glTFMaterial/glTFMaterialBase.h"

struct glTFSceneNode
{
    void MarkDirty() const;
    bool IsDirty() const;
    void ResetDirty() const;

    glTF_Transform_WithTRS m_transform;
    glTF_Transform_WithTRS m_finalTransform;

    std::vector<std::unique_ptr<glTFSceneObjectBase>> m_objects;
    std::vector<std::unique_ptr<glTFSceneNode>> m_children;
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

    template<typename SceneNodeType>
    std::vector<SceneNodeType*> GetAllTypedNodes() const
    {
        std::vector<SceneNodeType*> result;
        TraverseNodes([&, this](const glTFSceneNode& node)
        {
            for (const auto& obj : node.m_objects)
            {
                if (SceneNodeType* scene_node = dynamic_cast<SceneNodeType*>(obj.get()))
                {
                    result.push_back(scene_node);
                }
            }
            return true;
        });
        
        return result;
    }

    glTF_AABB::AABB GetBounds() const;
    
    const glTFSceneNode& GetRootNode() const;

    bool Init(const glTFLoader& loader);
    
protected:
    void TraverseNodesInner(const std::function<bool(glTFSceneNode&)>& visitor) const;
    void RecursiveInitSceneNodeFromGLTFLoader(const glTFLoader& loader, const glTFHandle& handle, const glTFSceneNode& parentNode, glTFSceneNode& sceneNode);

    std::unique_ptr<glTFSceneNode> m_root;

    std::map<glTFHandle, std::shared_ptr<glTFMaterialBase>> mesh_materials;
    std::map<unsigned, std::shared_ptr<glTFMeshRawData>> mesh_datas;
};
