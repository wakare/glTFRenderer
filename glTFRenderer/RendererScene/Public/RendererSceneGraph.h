#pragma once

#define NOMINMAX

#include <memory>
#include <glm/glm/glm.hpp>
#include <glm/glm/detail/type_quat.hpp>

#include "RendererCommon.h"
#include "RendererSceneAABB.h"
#include "RHICommon.h"
#include "SceneFileLoader/glTFLoader.h"

class RendererSceneMesh
{
public:
    RendererSceneMesh(const glTFLoader& loader, const glTF_Primitive& primitive);
    RendererSceneMesh(VertexLayoutDeclaration vertex_layout, std::shared_ptr<VertexBufferData> vertex_buffer, std::shared_ptr<IndexBufferData> index_buffer);
    
    RendererSceneAABB GetBoundingBox() const { return m_box; }
    const VertexLayoutDeclaration& GetLayout() const {return m_vertex_layout; }

    const VertexBufferData& GetVertexBuffer() const {return *m_vertex_buffer_data; }
    const VertexBufferData& GetPositionOnlyBuffer() const {return *m_position_only_data; }
    const IndexBufferData& GetIndexBuffer() const {return *m_index_buffer_data; }
    
protected:
    VertexLayoutDeclaration m_vertex_layout;
    
    std::shared_ptr<VertexBufferData> m_vertex_buffer_data;
    std::shared_ptr<VertexBufferData> m_position_only_data;
    std::shared_ptr<IndexBufferData> m_index_buffer_data;

    RendererSceneAABB m_box;
};

class RendererSceneNodeTransform
{
public:
    RendererSceneNodeTransform(const glm::fmat4& transform = {});
    
    void Translate(const glm::fvec3& translation);
    void TranslateOffset(const glm::fvec3& translation);
    void RotateEulerAngle(const glm::fvec3& euler_angle);
    void RotateEulerAngleOffset(const glm::fvec3& euler_angle);
    void Scale(const glm::fvec3& scale);

    void MarkTransformDirty();

    glm::fmat4 GetTransformMatrix();

    static std::shared_ptr<RendererSceneNodeTransform> identity_transform;
    
protected:
    glm::fvec3 m_translation{};
    glm::fvec3 m_euler_angles{};
    glm::fvec3 m_scale{};
    glm::quat m_quat{};

    glm::fmat4 m_cached_transform{};
    
    bool m_transform_dirty{true};
};

class RendererSceneNode : public glTFUniqueObject<RendererSceneNode>
{
public:
    RendererSceneNode(std::shared_ptr<RendererSceneNodeTransform> local_transform = RendererSceneNodeTransform::identity_transform, std::shared_ptr<RendererSceneMesh> mesh = nullptr);
    
    void MarkDirty();
    bool IsDirty() const;
    void ResetDirty();

    void SetMesh(std::shared_ptr<RendererSceneMesh> mesh);
    void SetLocalTransform(std::shared_ptr<RendererSceneNodeTransform> transform);

    bool HasMesh() const;
    
    const RendererSceneMesh& GetMesh() const;
    const RendererSceneNodeTransform& GetLocalTransform() const;

    void AddChild(std::shared_ptr<RendererSceneNode> child);
    
protected:
    bool m_dirty {true};
    std::shared_ptr<RendererSceneNodeTransform> m_local_transform;
    std::map<glTFUniqueID, std::shared_ptr<RendererSceneNode>> m_children;

    std::shared_ptr<RendererSceneMesh> m_mesh;
};

class RendererSceneGraph
{
public:
    RendererSceneGraph();
    
    bool AddRootNodeWithFile_glTF(const glTFLoader& loader);
    
protected:
    void RecursiveInitSceneNodeFromGLTFLoader(const glTFLoader& loader, const glTFHandle& handle, const RendererSceneNode& parent_node, RendererSceneNode& scene_node);
    std::shared_ptr<RendererSceneNode> m_root_node;
};
