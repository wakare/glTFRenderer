#include "glTFSceneTriangleMesh.h"

#include <utility>

glTFSceneTriangleMesh::glTFSceneTriangleMesh(
    const glTF_Transform_WithTRS& parentTransformRef,
    std::shared_ptr<VertexBufferData> vertexBufferData, std::shared_ptr<IndexBufferData> indexBufferData)
        : glTFScenePrimitive(parentTransformRef)
        , m_vertexBufferData(std::move(vertexBufferData))
        , m_indexBufferData(std::move(indexBufferData))
{
    
}

const VertexLayoutDeclaration& glTFSceneTriangleMesh::GetVertexLayout() const
{
    return m_vertexBufferData->layout;
}

const VertexBufferData& glTFSceneTriangleMesh::GetVertexBufferData() const
{
    return *m_vertexBufferData;
}

const IndexBufferData& glTFSceneTriangleMesh::GetIndexBufferData() const
{
    return *m_indexBufferData;
}
