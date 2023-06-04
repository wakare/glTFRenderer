#include "glTFPointLight.h"

glTFPointLight::glTFPointLight(const glm::vec3& position, float radius, float falloff)
    : glTFLocalLight(glTFLightType::PointLight, position)
    , m_radius(radius)
    , m_falloff(falloff)
{
    
}

bool glTFPointLight::AffectPosition(const glm::vec3& position) const
{
    return (GetPosition() - position).length() < m_radius;
}

void glTFPointLight::SetRadius(float radius)
{
    m_radius = radius;
}

float glTFPointLight::GetRadius() const
{
    return m_radius;
}

void glTFPointLight::SetFalloff(float falloff)
{
    m_falloff = falloff;
}

float glTFPointLight::GetFalloff() const
{
    return m_falloff;
}
