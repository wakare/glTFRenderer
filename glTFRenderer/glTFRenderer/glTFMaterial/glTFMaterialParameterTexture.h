#pragma once
#include <string>

#include "glTFMaterialCommon.h"
#include "glTFMaterialParameterBase.h"

class glTFMaterialParameterTexture : public glTFMaterialParameterBase
{
public:
    glTFMaterialParameterTexture(std::string texture_path, glTFMaterialParameterUsage usage);
    
    const std::string& GetTexturePath() const {return m_texture_path; }
    bool IsSRGB() const;
    
protected:
    std::string m_texture_path;
};