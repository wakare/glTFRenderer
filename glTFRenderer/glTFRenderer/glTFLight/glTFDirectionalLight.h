#pragma once
#include "glTFLightBase.h"

class glTFDirectionalLight : public glTFLightBase
{
public:
    glTFDirectionalLight(const glTF_Transform_WithTRS& parentTransformRef);
    
    virtual bool IsLocal() const override;
    bool AffectPosition(const glm::vec3& position) const override;

    glm::vec3 GetDirection() const;
};
