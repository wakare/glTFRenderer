#pragma once
#include "glTFScenePrimitive.h"
#include "SceneFileLoader/glTFElementCommon.h"
#include "SceneFileLoader/glTFLoader.h"

class glTFMeshRawData : public glTFUniqueObject<glTFMeshRawData>
{
public:
    glTFMeshRawData(const glTFLoader& loader, const glTF_Primitive& primitive);

    glTF_AABB::AABB GetBoundingBox() const { return m_box; }
    const VertexLayoutDeclaration& GetLayout() const {return vertexLayout; }

    const VertexBufferData& GetVertexBuffer() const {return *vertex_buffer_data; }
    const VertexBufferData& GetPositionOnlyBuffer() const {return *position_only_data; }
    const IndexBufferData& GetIndexBuffer() const {return *index_buffer_data; }
    
protected:
    VertexLayoutDeclaration vertexLayout;
    
    std::shared_ptr<VertexBufferData> vertex_buffer_data;
    std::shared_ptr<VertexBufferData> position_only_data;
    std::shared_ptr<IndexBufferData> index_buffer_data;

    glTF_AABB::AABB m_box;
};
