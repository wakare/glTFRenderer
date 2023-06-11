#pragma once
#include "glTFScenePrimitive.h"

class glTFSceneBox : public glTFScenePrimitive
{
public:
    glTFSceneBox(const glTF_Transform_WithTRS& parentTransformRef);
    virtual const VertexLayoutDeclaration& GetVertexLayout() const override;
    virtual const VertexBufferData& GetVertexBufferData() const override;
    virtual const IndexBufferData& GetIndexBufferData() const override;
    
private:
    VertexLayoutDeclaration m_vertexLayout;
    VertexBufferData m_vertexBufferData;
    IndexBufferData m_indexBufferData;
};
