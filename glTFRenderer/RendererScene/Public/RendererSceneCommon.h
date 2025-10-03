#pragma once

#include <map>
#include <string>
#include <glm/glm/glm.hpp>

#include "RendererCommon.h"

class MaterialParameter
{
public:
    enum class MaterialParameterType
    {
        FACTOR,
        TEXTURE,
    };

    MaterialParameter(glm::fvec4 factor);
    MaterialParameter(std::string texture);

    MaterialParameterType GetType() const;
    const std::string& GetTexture() const;
    const glm::fvec4& GetFactor() const;
    
protected:
    MaterialParameterType m_type;
    
    std::string m_texture_path;
    glm::fvec4 m_factor;
};

class MaterialBase : public RendererUniqueObjectIDBase<MaterialBase>
{
public:
    enum class MaterialParameterUsage
    {
        BASE_COLOR,
        NORMAL,
        METALLIC_ROUGHNESS,
        UNKNOWN,
    };

    void SetParameter(MaterialParameterUsage usage, std::shared_ptr<MaterialParameter> param);
    bool HasParameter(MaterialParameterUsage usage) const;
    std::shared_ptr<MaterialParameter> GetParameter(MaterialParameterUsage usage) const;
    
protected:
    std::map<MaterialParameterUsage, std::shared_ptr<MaterialParameter>> m_material_parameters;
};
