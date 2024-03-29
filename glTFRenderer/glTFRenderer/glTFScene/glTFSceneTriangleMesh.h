#pragma once
#include "glTFMeshRawData.h"
#include "glTFScenePrimitive.h"

class glTFMeshRawData;

class glTFSceneTriangleMesh : public glTFScenePrimitive
{
public:
    glTFSceneTriangleMesh(const glTF_Transform_WithTRS& parentTransformRef, std::shared_ptr<glTFMeshRawData> raw_data);
    
    virtual const VertexLayoutDeclaration& GetVertexLayout() const override;
    virtual const VertexBufferData& GetVertexBufferData() const override;
    virtual const IndexBufferData& GetIndexBufferData() const override;
    virtual const VertexBufferData& GetPositionOnlyBufferData() const override;
    virtual glTFUniqueID GetMeshRawDataID() const override { return m_raw_data->GetID(); }
    const glTFMeshRawData& GetMeshRawData() const {return *m_raw_data; }
    
private:
    std::shared_ptr<glTFMeshRawData> m_raw_data;
};
