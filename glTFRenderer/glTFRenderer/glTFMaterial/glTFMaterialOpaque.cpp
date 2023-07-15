#include "glTFMaterialOpaque.h"

#include <utility>

glTFMaterialOpaque::glTFMaterialOpaque()
    : glTFMaterialBase(MaterialType::Opaque)
{
}

void glTFMaterialOpaque::AddOrUpdateMaterialParameter(glTFMaterialParameterUsage usage, std::shared_ptr<glTFMaterialParameterTexture> parameter)
{
    m_parameters[usage] = std::move(parameter);
}

bool glTFMaterialOpaque::HasValidParameter(glTFMaterialParameterUsage usage) const
{
    return m_parameters.find(usage) != m_parameters.end();
}
