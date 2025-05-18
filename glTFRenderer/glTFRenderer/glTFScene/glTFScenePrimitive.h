#pragma once
#include <memory>
#include <vector>

#include "glTFSceneObjectBase.h"
#include "glTFMaterial/glTFMaterialBase.h"
#include "RHICommon.h"

enum class VertexAttributeType
{
    // Vertex
    VERTEX_POSITION,
    VERTEX_NORMAL,
    VERTEX_TANGENT,
    VERTEX_TEXCOORD0,

    // Instance
    INSTANCE_MAT_0,
    INSTANCE_MAT_1,
    INSTANCE_MAT_2,
    INSTANCE_MAT_3,
    INSTANCE_CUSTOM_DATA,
};

struct VertexAttributeElement
{
    VertexAttributeType type;
    unsigned byte_size;
};

struct VertexLayoutDeclaration
{
    std::vector<VertexAttributeElement> elements;

    bool HasAttribute(VertexAttributeType attribute_type) const
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
    VertexLayoutDeclaration layout;
    
    std::unique_ptr<char[]> data;
    size_t byte_size;
    size_t vertex_count;

    bool GetVertexAttributeDataByIndex(VertexAttributeType type, unsigned index, void* out_data, size_t& out_attribute_size) const
    {
        const char* start_data = data.get() + index * layout.GetVertexStrideInBytes();
        for (unsigned i = 0; i < layout.elements.size(); ++i)
        {
            if (type == layout.elements[i].type)
            {
                memcpy(out_data, start_data, layout.elements[i].byte_size);
                out_attribute_size = layout.elements[i].byte_size;
                return true;
            }

            start_data += layout.elements[i].byte_size;
        }

        out_attribute_size = 0;
        return false;
    }
};

struct IndexBufferData
{
    RHIDataFormat format;
    
    std::unique_ptr<char[]> data;
    size_t byte_size;
    size_t index_count;

    unsigned GetStride() const;
    unsigned GetIndexByOffset(size_t offset) const;
};

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
