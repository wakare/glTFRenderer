#include "glTFSceneTriangleMesh.h"

#include <utility>

glTFSceneTriangleMesh::glTFSceneTriangleMesh(
    const glTF_Transform_WithTRS& parentTransformRef,
    VertexLayoutDeclaration vertexLayout,
    std::shared_ptr<VertexBufferData> vertexBufferData, std::shared_ptr<IndexBufferData> indexBufferData)
        : glTFScenePrimitive(parentTransformRef)
        , m_vertexLayout(std::move(vertexLayout))
        , m_vertexBufferData(std::move(vertexBufferData))
        , m_indexBufferData(std::move(indexBufferData))
{
    
}

const VertexLayoutDeclaration& glTFSceneTriangleMesh::GetVertexLayout() const
{
    return m_vertexLayout;
}

const VertexBufferData& glTFSceneTriangleMesh::GetVertexBufferData() const
{
    return *m_vertexBufferData;
}

const IndexBufferData& glTFSceneTriangleMesh::GetIndexBufferData() const
{
    return *m_indexBufferData;
}
