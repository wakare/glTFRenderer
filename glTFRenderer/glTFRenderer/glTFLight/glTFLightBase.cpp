#include "glTFLightBase.h"

glTFLightBase::glTFLightBase(glTFLightType type)
    : m_type(type)
    , m_intensity(1.0f)
{
}

glTFLightType glTFLightBase::GetType() const
{
    return m_type;
}

bool glTFLightBase::IsLocal() const
{
    return m_type == glTFLightType::PointLight ||
        m_type == glTFLightType::SpotLight;
}

void glTFLightBase::SetIntensity(float intensity)
{
    m_intensity = intensity;
}

float glTFLightBase::GetIntensity() const
{
    return m_intensity;
}
