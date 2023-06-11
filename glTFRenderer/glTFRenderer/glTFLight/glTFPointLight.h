#pragma once
#include "glTFLocalLight.h"

class glTFPointLight : public glTFLocalLight
{
public:
    glTFPointLight(const glTF_Transform_WithTRS& parentTransformRef,const glm::vec3& position = {0.0f, 0.0f, 0.0f}, float radius = 0.0f, float falloff = 2.0f);

    virtual bool AffectPosition(const glm::vec3& position) const override;
    
    void SetRadius(float radius);
    float GetRadius() const;

    void SetFalloff(float falloff);
    float GetFalloff() const;
    
private:
    float m_radius;
    float m_falloff;
};
