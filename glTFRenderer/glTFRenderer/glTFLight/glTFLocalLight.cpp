#include "glTFLocalLight.h"

glTFLocalLight::glTFLocalLight(glTFLightType type, const glm::vec3& position)
    : glTFLightBase(type)
    , m_position(position)
{
}

bool glTFLocalLight::IsLocal() const
{
    return true;
}

void glTFLocalLight::SetPosition(const glm::vec3& position)
{
    m_position = position;
}

const glm::vec3& glTFLocalLight::GetPosition() const
{
    return m_position;
}
