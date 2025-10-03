#include "RendererSceneCommon.h"


MaterialParameter::MaterialParameter(glm::fvec4 factor)
    : m_type(MaterialParameterType::FACTOR)
    , m_texture_path("")
    , m_factor(factor)
{
}

MaterialParameter::MaterialParameter(std::string texture)
    : m_type(MaterialParameterType::TEXTURE)
    , m_texture_path(std::move(texture))
    , m_factor(1.0f)
{
    
}

MaterialParameter::MaterialParameterType MaterialParameter::GetType() const
{
    return m_type;
}

const std::string& MaterialParameter::GetTexture() const
{
    return m_texture_path;
}

const glm::fvec4& MaterialParameter::GetFactor() const
{
    return m_factor;
}

void MaterialBase::SetParameter(MaterialParameterUsage usage, std::shared_ptr<MaterialParameter> param)
{
    m_material_parameters[usage] = std::move(param);
}

bool MaterialBase::HasParameter(MaterialParameterUsage usage) const
{
    return m_material_parameters.contains(usage);
}

std::shared_ptr<MaterialParameter> MaterialBase::GetParameter(MaterialParameterUsage usage) const
{
    return m_material_parameters.at(usage);
}
