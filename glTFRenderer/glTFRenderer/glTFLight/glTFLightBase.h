#pragma once
#include <vec3.hpp>

#include "../glTFScene/glTFSceneObjectBase.h"
#include "../glTFUtils/glTFUtils.h"

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
    
protected:
    glTFLightType m_type;
    float m_intensity;
};
