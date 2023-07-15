#pragma once
#include <memory>
#include <map>

#include "glTFMaterialBase.h"
#include "glTFMaterialParameterTexture.h"

class glTFMaterialOpaque : public glTFMaterialBase
{
public:
    glTFMaterialOpaque();
    void AddOrUpdateMaterialParameter(glTFMaterialParameterUsage usage, std::shared_ptr<glTFMaterialParameterTexture> parameter);
    bool HasValidParameter(glTFMaterialParameterUsage usage) const;
    
private:
    std::map<glTFMaterialParameterUsage, std::shared_ptr<glTFMaterialParameterTexture>> m_parameters;
};
