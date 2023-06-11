#include "glTFDirectionalLight.h"

glTFDirectionalLight::glTFDirectionalLight(const glTF_Transform_WithTRS& parentTransformRef)
    : glTFLightBase(parentTransformRef, glTFLightType::DirectionalLight)
{
    
}

bool glTFDirectionalLight::IsLocal() const
{
    return false;
}

bool glTFDirectionalLight::AffectPosition(const glm::vec3& position) const
{
    return true;
}

glm::vec3 glTFDirectionalLight::GetDirection() const
{
    glm::vec4 local = {0.0f, -1.0f, 0.0f, 0.0f};
    local = GetTransformMatrix() * local;
    return glm::vec3(local);
}
