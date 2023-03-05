#pragma once
#include <memory>
#include <vector>

#include "glTFSceneObjectBase.h"

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
};

struct IndexBufferData
{
    std::unique_ptr<unsigned[]> data;
    size_t byteSize;
};

class glTFScenePrimitive : public glTFSceneObjectBase
{
public:
    glTFScenePrimitive() = default;

    virtual const VertexLayoutDeclaration& GetVertexLayout() const = 0;
    virtual const VertexBufferData& GetVertexBufferData() const = 0;
    virtual const IndexBufferData& GetIndexBufferData() const = 0;
};
