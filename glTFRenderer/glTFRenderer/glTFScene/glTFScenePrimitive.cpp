#include "glTFScenePrimitive.h"

#include <utility>

#include "glTFMaterial/glTFMaterialOpaque.h"
#include <Windows.h>

unsigned IndexBufferData::GetStride() const
{
    if (format == RHIDataFormat::R16_UINT)
    {
        return sizeof(USHORT);
    }
    else if (format == RHIDataFormat::R32_UINT)
    {
        return sizeof(UINT);
        
    }
    GLTF_CHECK(false);
    
    return 0; 
}

unsigned IndexBufferData::GetIndexByOffset(size_t offset) const
{
    char* index_data = data.get() + GetStride() * offset;
    if (format == RHIDataFormat::R16_UINT)
    {
        const USHORT* index_data_ushort = reinterpret_cast<USHORT*>(index_data);
        return *index_data_ushort;
    }
    else if (format == RHIDataFormat::R32_UINT)
    {
        const UINT* index_data_uint = reinterpret_cast<UINT*>(index_data);
        return *index_data_uint;
    }
    
    GLTF_CHECK(false);
    return 0;
}

void glTFScenePrimitive::SetMaterial(std::shared_ptr<glTFMaterialBase> material)
{
    m_material = std::move(material);
}

bool glTFScenePrimitive::HasNormalMapping() const
{
    if (!m_primitive_flags.IsNormalMapping())
    {
        return false;
    }
    
    const bool contains_tangent = GetVertexLayout().HasAttribute(VertexAttributeType::VERTEX_TANGENT);
    bool contains_normal_texture = false;
    if (HasMaterial() && GetMaterial().GetMaterialType() == MaterialType::Opaque)
    {
        contains_normal_texture = dynamic_cast<const glTFMaterialOpaque&>(GetMaterial()).HasValidParameter(glTFMaterialParameterUsage::NORMAL);    
    }
    
    return contains_tangent && contains_normal_texture;
}

const glTFMaterialBase& glTFScenePrimitive::GetMaterial() const
{
    return *m_material;
}
