#pragma once
#include <memory>
#include <vector>

#include "glTFSceneObjectBase.h"
#include "../glTFMaterial/glTFMaterialBase.h"

enum class VertexLayoutType
{
    POSITION,
    NORMAL,
    UV,
};

struct VertexLayoutElement
{
    VertexLayoutType type;
    unsigned byteSize;
};

struct VertexLayoutDeclaration
{
    std::vector<VertexLayoutElement> elements;

    size_t GetVertexStride() const
    {
        size_t stride = 0;
        for (const auto& element : elements)
        {
            stride += element.byteSize;
        }

        return stride;
    }
};

struct VertexBufferData
{
    std::unique_ptr<char[]> data;
    size_t byteSize;
    size_t vertexCount;
};

enum class IndexBufferElementType
{
    UNSIGNED_SHORT,
    UNSIGNED_INT,
};

struct IndexBufferData
{
    IndexBufferElementType elementType;
    
    std::unique_ptr<char[]> data;
    size_t byteSize;
    size_t indexCount;
};

class glTFScenePrimitive : public glTFSceneObjectBase
{
public:
    glTFScenePrimitive(const glTF_Transform_WithTRS& parentTransformRef,
                       std::shared_ptr<glTFMaterialBase> m_material = nullptr)
        : glTFSceneObjectBase(parentTransformRef),
          m_material(std::move(m_material))
    {
    }

    virtual const VertexLayoutDeclaration& GetVertexLayout() const = 0;
    virtual const VertexBufferData& GetVertexBufferData() const = 0;
    virtual const IndexBufferData& GetIndexBufferData() const = 0;

    virtual size_t GetInstanceCount() const { return 1; }
    void SetMaterial(std::shared_ptr<glTFMaterialBase> material);
    bool HasMaterial() const {return m_material != nullptr; }
    const glTFMaterialBase& GetMaterial() const;
    
private:
    std::shared_ptr<glTFMaterialBase> m_material;
};
