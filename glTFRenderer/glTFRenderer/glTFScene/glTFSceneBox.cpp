#include "glTFSceneBox.h"

glTFSceneBox::glTFSceneBox()
{
    m_vertexLayout.elements.push_back({VertexLayoutType::POSITION, 12});
    m_vertexLayout.elements.push_back({VertexLayoutType::UV, 8});

    /*
    constexpr float boxVertices[] =
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
    */
    constexpr float boxVertices[] =
    {
        -0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
         0.5f, -0.5f, 0.5f, 1.0f, 0.0f,
        -0.5f,  0.5f, 0.5f, 0.0f, 1.0f,
         0.5f,  0.5f, 0.5f, 1.0f, 1.0f,
    };

    const unsigned boxIndices[] =
    {
        0, 2, 1,
        1, 2, 3,
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

