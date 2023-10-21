#include "glTFMaterialParameterTexture.h"

glTFMaterialParameterTexture::glTFMaterialParameterTexture(std::string texture_path, glTFMaterialParameterUsage usage)
    : glTFMaterialParameterBase(Texture, usage)
    , m_texture_path(std::move(texture_path))       
{
        
}

bool glTFMaterialParameterTexture::IsSRGB() const
{
    return m_parameter_usage == glTFMaterialParameterUsage::BASECOLOR;
}
