#include "glTFMaterialOpaque.h"

glTFMaterialOpaque::glTFMaterialOpaque()
    : glTFMaterialBase(MaterialType::Opaque)
    , m_albedo({0.0f, 0.0f, 0.0f})
    , m_albedoTexture(nullptr)
{
}

glTFMaterialOpaque::glTFMaterialOpaque(const glm::vec3& albedo)
    : glTFMaterialBase(MaterialType::Opaque)
    , m_albedo(albedo)
    , m_albedoTexture(nullptr)
{
}

glTFMaterialOpaque::glTFMaterialOpaque(std::shared_ptr<glTFMaterialTexture> albedoTexture)
    : glTFMaterialBase(MaterialType::Opaque)
    , m_albedo({0.0f, 0.0f, 0.0f})
    , m_albedoTexture(std::move(albedoTexture))
{
}
