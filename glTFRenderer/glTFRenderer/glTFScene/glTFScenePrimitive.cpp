#include "glTFScenePrimitive.h"

#include <utility>

#include "glTFMaterial/glTFMaterialOpaque.h"

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
    
    const bool contains_tangent = GetVertexLayout().HasAttribute(VertexLayoutType::TANGENT);
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
