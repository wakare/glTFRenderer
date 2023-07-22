#include "glTFMaterialOpaque.h"

#include <utility>

#include "../glTFLoader/glTFElementCommon.h"

glTFMaterialOpaque::glTFMaterialOpaque()
    : glTFMaterialBase(MaterialType::Opaque)
{
}

void glTFMaterialOpaque::AddOrUpdateMaterialParameterTexture(glTFMaterialParameterUsage usage, std::shared_ptr<glTFMaterialParameterTexture> texture)
{
    m_parameter_textures[usage] = std::move(texture);
}

bool glTFMaterialOpaque::HasValidParameterTexture(glTFMaterialParameterUsage usage) const
{
    return m_parameter_textures.find(usage) != m_parameter_textures.end();
}

void glTFMaterialOpaque::AddOrUpdateMaterialParameterFactor(glTFMaterialParameterUsage usage,
    std::shared_ptr<glTFMaterialParameterBase> factor)
{
    m_parameter_factors[usage] = std::move(factor);
}

bool glTFMaterialOpaque::HasValidParameterFactor(glTFMaterialParameterUsage usage) const
{
    return m_parameter_factors.find(usage) != m_parameter_factors.end();
}

void glTFMaterialOpaque::AddOrUpdateMaterialParameter(glTFMaterialParameterUsage usage,
                                                      std::shared_ptr<glTFMaterialParameterBase> parameter)
{
    switch (parameter->GetParameterType())
    {
        case Factor:
            {
                AddOrUpdateMaterialParameterFactor(usage, parameter);
            }
            break;
    	case Texture:
    	    {
    	        AddOrUpdateMaterialParameterTexture(usage, std::dynamic_pointer_cast<glTFMaterialParameterTexture>(parameter));
    	    }
    	    break;
    default: ; }
}

bool glTFMaterialOpaque::HasValidParameter(glTFMaterialParameterUsage usage) const
{
    return HasValidParameterFactor(usage) || HasValidParameterTexture(usage);
}

const glTFMaterialParameterBase& glTFMaterialOpaque::GetMaterialParameter(glTFMaterialParameterUsage usage) const
{
    GLTF_CHECK(HasValidParameter(usage));

    glTFMaterialParameterBase* result = nullptr;
	if (HasValidParameterTexture(usage))
	{
        const auto it_material_parameter = m_parameter_textures.find(usage);
	    result = it_material_parameter->second.get();
	}
    else if (HasValidParameterFactor(usage))
    {
        const auto it_material_parameter = m_parameter_factors.find(usage);
        result = it_material_parameter->second.get();
    }

    GLTF_CHECK(result);
    return *result; 
}
