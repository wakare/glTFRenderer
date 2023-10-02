#pragma once
#include "glTFScenePrimitive.h"

class glTFSceneTriangleMesh : public glTFScenePrimitive
{
public:
    glTFSceneTriangleMesh(const glTF_Transform_WithTRS& parentTransformRef,
        std::shared_ptr<VertexBufferData> vertexBufferData,
        std::shared_ptr<IndexBufferData> indexBufferData);
    
    virtual const VertexLayoutDeclaration& GetVertexLayout() const override;
    virtual const VertexBufferData& GetVertexBufferData() const override;
    virtual const IndexBufferData& GetIndexBufferData() const override;

private:
    std::shared_ptr<VertexBufferData> m_vertexBufferData;
    std::shared_ptr<IndexBufferData> m_indexBufferData;
};