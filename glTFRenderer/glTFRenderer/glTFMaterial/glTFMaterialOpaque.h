#pragma once
#include <memory>
#include <vec3.hpp>

#include "glTFMaterialBase.h"
#include "glTFMaterialTexture.h"

class glTFMaterialOpaque : public glTFMaterialBase
{
public:
    glTFMaterialOpaque();
    glTFMaterialOpaque(const glm::vec3& albedo);
    glTFMaterialOpaque(std::shared_ptr<glTFMaterialTexture> albedoTexture);
    
    void SetAlbedo(const glm::vec3& albedo) { m_albedo = albedo; }
    const glm::vec3& GetAlbedo() const {return m_albedo; }
    
    bool UsingAlbedoTexture() const {return m_albedoTexture != nullptr; }
    const glTFMaterialTexture& GetAlbedoTexture() const
    {
        assert(m_albedoTexture);
        return *m_albedoTexture;
    }
    
private:
    glm::vec3 m_albedo;
    std::shared_ptr<glTFMaterialTexture> m_albedoTexture;
};
