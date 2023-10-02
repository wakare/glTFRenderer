#include "glTFSceneBox.h"

glTFSceneBox::glTFSceneBox(const glTF_Transform_WithTRS& parentTransformRef)
    : glTFScenePrimitive(parentTransformRef)
{
    

    /*
     *   6 --  -- 7
     *  /|       /|
     * 2 --  -- 3 |
     * | |      | |
     * | 4 --  -| 5
     * |/       |/
     * 0 --  -- 1
     */
    
    constexpr float boxVertices[] =
    {
        -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 
         1.0f, -1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, -1.0f, 0.0f, 1.0f, 
         1.0f,  1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f, -1.0f,  1.0f, 1.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f, 1.0f,
        -1.0f,  1.0f,  1.0f, 1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 0.0f, 0.0f,
    };

    constexpr float boxVerticesPositionOnly[] =
    {
        -1.0f, -1.0f, -1.0f, 
         1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f, 
         1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
    };
    
    const unsigned boxIndices[] =
    {
        0, 2, 1,
        1, 2, 3,
        0, 4, 2,
        2, 4, 6,
        0, 1, 5,
        0, 5, 4,
        1, 3, 5,
        3, 7, 5,
        2, 6, 7,
        2, 7, 3,
        4, 5, 7,
        4, 7, 6
    };
    
    m_vertex_buffer_data.data.reset(new char[sizeof(boxVertices)]);
    memcpy(m_vertex_buffer_data.data.get(), boxVertices, sizeof(boxVertices));
    m_vertex_buffer_data.byteSize = sizeof(boxVertices);
    m_vertex_buffer_data.vertex_count = 8;
    m_vertex_buffer_data.layout.elements.push_back({VertexLayoutType::POSITION, 12});
    m_vertex_buffer_data.layout.elements.push_back({VertexLayoutType::TEXCOORD_0, 8});
    
    m_indexBufferData.data.reset(new char[sizeof(boxIndices)]);
    memcpy(m_indexBufferData.data.get(), boxIndices, sizeof(boxIndices));
    m_indexBufferData.byteSize = sizeof(boxIndices);
    m_indexBufferData.index_count = sizeof(boxIndices) / sizeof(unsigned);
    m_indexBufferData.format = RHIDataFormat::R32_UINT;

    m_position_only_buffer_data.data.reset(new char[sizeof(boxVerticesPositionOnly)]);
    memcpy(m_position_only_buffer_data.data.get(), boxVerticesPositionOnly, sizeof(boxVerticesPositionOnly));
    m_position_only_buffer_data.byteSize = sizeof(boxVerticesPositionOnly);
    m_position_only_buffer_data.vertex_count = 8;
    m_position_only_buffer_data.layout.elements.push_back({VertexLayoutType::POSITION, 12});
}

const VertexLayoutDeclaration& glTFSceneBox::GetVertexLayout() const
{
    return m_vertex_buffer_data.layout;
}

const VertexBufferData& glTFSceneBox::GetVertexBufferData() const
{
    return m_vertex_buffer_data;
}

const IndexBufferData& glTFSceneBox::GetIndexBufferData() const
{
    return m_indexBufferData;
}

const VertexBufferData& glTFSceneBox::GetPositionOnlyBufferData() const
{
    return m_position_only_buffer_data;
}

