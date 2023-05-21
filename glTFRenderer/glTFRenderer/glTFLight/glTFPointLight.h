#pragma once
#include "glTFLocalLight.h"

class glTFPointLight : public glTFLocalLight
{
public:
    glTFPointLight(const glm::vec3& position = {0.0f, 0.0f, 0.0f}, float radius = 0.0f);

    virtual bool AffectPosition(const glm::vec3& position) const override;
    
private:
    float m_radius;
};
