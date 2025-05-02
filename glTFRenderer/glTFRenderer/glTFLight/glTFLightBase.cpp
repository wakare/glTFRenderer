#include "glTFLightBase.h"

glTFLightBase::glTFLightBase(const glTF_Transform_WithTRS& parentTransformRef, glTFLightType type)
    : glTFSceneObjectBase(parentTransformRef)
    , m_type(type)
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

void glTFLightBase::SetColor(const glm::vec3& color)
{
    m_color = color;
}

const glm::vec3& glTFLightBase::GetColor() const
{
    return m_color;
}

LightShadowmapViewInfo glTFLightBase::GetShadowmapViewInfo(const glTF_AABB::AABB& scene_bounds, const LightShadowmapViewRange& range) const
{
    GLTF_CHECK(false);
    return {};
}
