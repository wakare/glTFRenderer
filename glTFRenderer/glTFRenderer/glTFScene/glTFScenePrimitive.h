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
    std::unique_ptr<float[]> data;
    size_t byteSize;

    size_t VertexCount() const {return byteSize / sizeof(float); }
};

struct IndexBufferData
{
    std::unique_ptr<unsigned[]> data;
    size_t byteSize;

    size_t IndexCount() const {return byteSize / sizeof(unsigned); }
};

class glTFScenePrimitive : public glTFSceneObjectBase
{
public:
    glTFScenePrimitive() = default;

    virtual const VertexLayoutDeclaration& GetVertexLayout() const = 0;
    virtual const VertexBufferData& GetVertexBufferData() const = 0;
    virtual const IndexBufferData& GetIndexBufferData() const = 0;

    virtual size_t GetInstanceCount() const { return 1; }
    void SetMaterial(std::shared_ptr<glTFMaterialBase> material);
    const glTFMaterialBase& GetMaterial() const;
    
private:
    std::shared_ptr<glTFMaterialBase> m_material;
};
