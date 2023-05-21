#include "glTFLightBase.h"

glTFLightBase::glTFLightBase(glTFLightType type)
    : m_type(type)
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
