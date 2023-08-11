#pragma once
#include <memory>
#include <vector>

#include "glTFSceneObjectBase.h"
#include "../glTFMaterial/glTFMaterialBase.h"

enum class VertexLayoutType
{
    POSITION,
    NORMAL,
    TANGENT,
    TEXCOORD_0,
};

struct VertexLayoutElement
{
    VertexLayoutType type;
    unsigned byte_size;
};

struct VertexLayoutDeclaration
{
    std::vector<VertexLayoutElement> elements;

    bool HasAttribute(VertexLayoutType attribute_type) const
    {
        for (const auto& element : elements)
        {
            if (element.type == attribute_type)
            {
                return true;
            }
        }
        return false;
    }
    
    size_t GetVertexStrideInBytes() const
    {
        size_t stride = 0;
        for (const auto& element : elements)
        {
            stride += element.byte_size;
        }

        return stride;
    }

    bool operator==(const VertexLayoutDeclaration& lhs) const
    {
        if (elements.size() != lhs.elements.size())
        {
            return false;
        }

        for (size_t i = 0; i < elements.size(); ++i)
        {
            if (elements[i].type != lhs.elements[i].type ||
                elements[i].byte_size != lhs.elements[i].byte_size)
            {
                return false;
            }
        }

        return true;
    }
};

struct VertexBufferData
{
    std::unique_ptr<char[]> data;
    size_t byteSize;
    size_t vertex_count;
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
    size_t index_count;
};

class glTFScenePrimitive : public glTFSceneObjectBase
{
public:
    glTFScenePrimitive(const glTF_Transform_WithTRS& parentTransformRef,
                       std::shared_ptr<glTFMaterialBase> m_material = nullptr)
        : glTFSceneObjectBase(parentTransformRef)
        , m_material(std::move(m_material))
    {
    }

    virtual const VertexLayoutDeclaration& GetVertexLayout() const = 0;
    virtual const VertexBufferData& GetVertexBufferData() const = 0;
    virtual const IndexBufferData& GetIndexBufferData() const = 0;

    virtual size_t GetInstanceCount() const { return 1; }
    void SetMaterial(std::shared_ptr<glTFMaterialBase> material);
    bool HasMaterial() const {return m_material != nullptr; }
    bool HasNormalMapping() const { return GetVertexLayout().HasAttribute(VertexLayoutType::TANGENT);}
    const glTFMaterialBase& GetMaterial() const;
    
private:
    std::shared_ptr<glTFMaterialBase> m_material;
};
