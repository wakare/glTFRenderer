#pragma once
#include <memory>
#include <vector>

#include "glTFSceneObjectBase.h"
#include "glTFMaterial/glTFMaterialBase.h"
#include "RHICommon.h"

enum glTFScenePrimitiveFlags
{
    glTFScenePrimitiveFlags_NormalMapping = 1 << 0,
};

struct glTFScenePrimitiveRenderFlags : public glTFFlagsBase<glTFScenePrimitiveFlags>
{
    void SetEnableNormalMapping(bool enable)
    {
        if (enable)
        {
            SetFlag(glTFScenePrimitiveFlags_NormalMapping);
        }
        else
        {
            UnsetFlag(glTFScenePrimitiveFlags_NormalMapping);
        }
    }
    
    bool IsNormalMapping() const {return IsFlagSet(glTFScenePrimitiveFlags_NormalMapping); }
};


class glTFScenePrimitive : public glTFSceneObjectBase
{
public:
    glTFScenePrimitive(const glTF_Transform_WithTRS& parentTransformRef,
                       std::shared_ptr<glTFMaterialBase> m_material = nullptr)
        : glTFSceneObjectBase(parentTransformRef)
        , m_material(std::move(m_material))
    {
        m_primitive_flags.SetEnableNormalMapping(true);
    }

    virtual const VertexLayoutDeclaration& GetVertexLayout() const = 0;
    virtual const VertexBufferData& GetVertexBufferData() const = 0;
    virtual const IndexBufferData& GetIndexBufferData() const = 0;
    virtual const VertexBufferData& GetPositionOnlyBufferData() const = 0;

    virtual glTFUniqueID GetMeshRawDataID() const { return 0; } 
    
    void SetMaterial(std::shared_ptr<glTFMaterialBase> material);
    bool HasMaterial() const {return m_material != nullptr; }
    bool HasNormalMapping() const;
    const glTFMaterialBase& GetMaterial() const;

private:
    glTFScenePrimitiveRenderFlags m_primitive_flags;
    std::shared_ptr<glTFMaterialBase> m_material;
};
