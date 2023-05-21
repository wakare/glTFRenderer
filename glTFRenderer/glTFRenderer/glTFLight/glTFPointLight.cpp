#include "glTFPointLight.h"

glTFPointLight::glTFPointLight(const glm::vec3& position, float radius)
    : glTFLocalLight(glTFLightType::PointLight, position)
    , m_radius(radius)
{
    
}

bool glTFPointLight::AffectPosition(const glm::vec3& position) const
{
    return (GetPosition() - position).length() < m_radius;
}
