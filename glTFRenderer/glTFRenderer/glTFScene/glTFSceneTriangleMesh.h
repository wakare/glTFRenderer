#pragma once
#include "glTFScenePrimitive.h"

class glTFSceneTriangleMesh : public glTFScenePrimitive
{
public:
    glTFSceneTriangleMesh(const glTF_Transform_WithTRS& parentTransformRef,
        std::shared_ptr<VertexBufferData> vertex_buffer_data,
        std::shared_ptr<IndexBufferData> index_buffer_data,
        std::shared_ptr<VertexBufferData> position_only_buffer_data);
    
    virtual const VertexLayoutDeclaration& GetVertexLayout() const override;
    virtual const VertexBufferData& GetVertexBufferData() const override;
    virtual const IndexBufferData& GetIndexBufferData() const override;
    virtual const VertexBufferData& GetPositionOnlyBufferData() const override;
    
private:
    std::shared_ptr<VertexBufferData> m_vertex_buffer_data;
    std::shared_ptr<IndexBufferData> m_index_buffer_data;
    std::shared_ptr<VertexBufferData> m_position_only_buffer_data;
};