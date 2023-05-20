#include "glTFScenePrimitive.h"

#include <utility>

void glTFScenePrimitive::SetMaterial(std::shared_ptr<glTFMaterialBase> material)
{
    m_material = std::move(material);
}

const glTFMaterialBase& glTFScenePrimitive::GetMaterial() const
{
    return *m_material;
}
