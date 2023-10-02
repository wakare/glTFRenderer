#include "glTFSceneTriangleMesh.h"

#include <utility>

glTFSceneTriangleMesh::glTFSceneTriangleMesh(
    const glTF_Transform_WithTRS& parentTransformRef,
    std::shared_ptr<VertexBufferData> vertex_buffer_data,
    std::shared_ptr<IndexBufferData> index_buffer_data,
    std::shared_ptr<VertexBufferData> position_only_buffer_data)
        : glTFScenePrimitive(parentTransformRef)
        , m_vertex_buffer_data(std::move(vertex_buffer_data))
        , m_index_buffer_data(std::move(index_buffer_data))
        , m_position_only_buffer_data(std::move(position_only_buffer_data))
{
    
}

const VertexLayoutDeclaration& glTFSceneTriangleMesh::GetVertexLayout() const
{
    return m_vertex_buffer_data->layout;
}

const VertexBufferData& glTFSceneTriangleMesh::GetVertexBufferData() const
{
    return *m_vertex_buffer_data;
}

const IndexBufferData& glTFSceneTriangleMesh::GetIndexBufferData() const
{
    return *m_index_buffer_data;
}

const VertexBufferData& glTFSceneTriangleMesh::GetPositionOnlyBufferData() const
{
    return *m_position_only_buffer_data;
}
