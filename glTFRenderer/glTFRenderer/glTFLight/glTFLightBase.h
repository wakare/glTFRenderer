#pragma once
#include <vec3.hpp>

#include "../glTFUtils/glTFUtils.h"

enum class glTFLightType
{
    PointLight,
    DirectionalLight,
    SpotLight,
    SkyLight,
    
};

class glTFLightBase : public glTFUniqueObject
{
public:
    glTFLightBase(glTFLightType type);

    glTFLightType GetType() const;
    bool IsLocal() const;

    virtual bool AffectPosition(const glm::vec3& position) const = 0;
    
private:
    glTFLightType m_type;
};
