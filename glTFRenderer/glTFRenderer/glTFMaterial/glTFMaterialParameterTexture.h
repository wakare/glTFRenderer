#pragma once
#include <string>
#include <memory>

#include "glTFMaterialCommon.h"
#include "glTFMaterialParameterBase.h"

class IRHIDescriptorHeap;
class glTFRenderResourceManager;

class glTFMaterialParameterTexture : public glTFMaterialParameterBase
{
public:
    glTFMaterialParameterTexture(std::string texture_path, glTFMaterialParameterUsage usage)
        : glTFMaterialParameterBase(glTFMaterialParameterType::Texture)
        , m_texture_path(std::move(texture_path))
        , m_texture_usage(usage)
    {
        
    }
    
    const std::string& GetTexturePath() const {return m_texture_path; }
    
protected:
    std::string m_texture_path;
    glTFMaterialParameterUsage m_texture_usage;
};