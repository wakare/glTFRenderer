#pragma once
#include <memory>
#include <map>

#include "glTFMaterialBase.h"
#include "glTFMaterialParameterTexture.h"

class glTFMaterialOpaque : public glTFMaterialBase
{
public:
    glTFMaterialOpaque();
    
	void AddOrUpdateMaterialParameter(glTFMaterialParameterUsage usage, std::shared_ptr<glTFMaterialParameterBase> parameter);
    bool HasValidParameter(glTFMaterialParameterUsage usage) const;
    const glTFMaterialParameterBase& GetMaterialParameter(glTFMaterialParameterUsage usage) const;
    
protected:
    void AddOrUpdateMaterialParameterTexture(glTFMaterialParameterUsage usage, std::shared_ptr<glTFMaterialParameterTexture> texture);
    bool HasValidParameterTexture(glTFMaterialParameterUsage usage) const;

    void AddOrUpdateMaterialParameterFactor(glTFMaterialParameterUsage usage, std::shared_ptr<glTFMaterialParameterBase> factor);
    bool HasValidParameterFactor(glTFMaterialParameterUsage usage) const;
    
    std::map<glTFMaterialParameterUsage, std::shared_ptr<glTFMaterialParameterTexture>> m_parameter_textures;
    std::map<glTFMaterialParameterUsage, std::shared_ptr<glTFMaterialParameterBase>> m_parameter_factors;
};
