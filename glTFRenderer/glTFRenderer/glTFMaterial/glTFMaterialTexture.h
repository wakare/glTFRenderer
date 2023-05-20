#pragma once
#include <string>

class glTFMaterialTexture
{
public:
    glTFMaterialTexture(std::string texturePath)
        : m_texturePath(std::move(texturePath))
    {
        
    }
    
    const std::string& GetTexturePath() const {return m_texturePath; }
    
private:
    std::string m_texturePath;
};
