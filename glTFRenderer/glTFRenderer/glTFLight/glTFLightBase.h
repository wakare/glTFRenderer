#pragma once
#include <vec3.hpp>
#include "glTFScene/glTFSceneObjectBase.h"

enum class glTFLightType
{
    PointLight,
    DirectionalLight,
    SpotLight,
    SkyLight,
    
};

class glTFLightBase : public glTFSceneObjectBase
{
public:
    glTFLightBase(const glTF_Transform_WithTRS& parentTransformRef, glTFLightType type);

    glTFLightType GetType() const;
    virtual bool IsLocal() const = 0;

    virtual bool AffectPosition(const glm::vec3& position) const = 0;
    void SetIntensity(float intensity);
    float GetIntensity() const;

    void SetColor(const glm::vec3& color);
    const glm::vec3& GetColor() const;
    
protected:
    glTFLightType m_type;
    glm::vec3 m_color;
    float m_intensity;
};
