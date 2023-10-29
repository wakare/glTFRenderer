#include "glTFSceneTriangleMesh.h"

#include <utility>

#include "glTFMeshRawData.h"

glTFSceneTriangleMesh::glTFSceneTriangleMesh(const glTF_Transform_WithTRS& parentTransformRef, std::shared_ptr<glTFMeshRawData> raw_data)
        : glTFScenePrimitive(parentTransformRef)
        , m_raw_data(std::move(raw_data))
{
    
}

const VertexLayoutDeclaration& glTFSceneTriangleMesh::GetVertexLayout() const
{
    return m_raw_data->GetLayout();
}

const VertexBufferData& glTFSceneTriangleMesh::GetVertexBufferData() const
{
    return m_raw_data->GetVertexBuffer();
}

const IndexBufferData& glTFSceneTriangleMesh::GetIndexBufferData() const
{
    return m_raw_data->GetIndexBuffer();
}

const VertexBufferData& glTFSceneTriangleMesh::GetPositionOnlyBufferData() const
{
    return m_raw_data->GetPositionOnlyBuffer();
}
