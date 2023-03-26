#include "glTFSceneBox.h"

glTFSceneBox::glTFSceneBox()
{
    m_vertexLayout.elements.push_back({VertexLayoutType::POSITION, 12});
    m_vertexLayout.elements.push_back({VertexLayoutType::UV, 8});

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
    
    m_vertexBufferData.data.reset(new float[sizeof(boxVertices) / sizeof(float)]);
    memcpy(m_vertexBufferData.data.get(), boxVertices, sizeof(boxVertices));
    m_vertexBufferData.byteSize = sizeof(boxVertices);
    
    m_indexBufferData.data.reset(new unsigned[sizeof(boxIndices) / sizeof(unsigned)]);
    memcpy(m_indexBufferData.data.get(), boxIndices, sizeof(boxIndices));
    m_indexBufferData.byteSize = sizeof(boxIndices);
}

const VertexLayoutDeclaration& glTFSceneBox::GetVertexLayout() const
{
    return m_vertexLayout;
}

const VertexBufferData& glTFSceneBox::GetVertexBufferData() const
{
    return m_vertexBufferData;
}

const IndexBufferData& glTFSceneBox::GetIndexBufferData() const
{
    return m_indexBufferData;
}

