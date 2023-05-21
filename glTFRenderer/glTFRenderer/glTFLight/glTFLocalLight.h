#pragma once
#include <vec3.hpp>

#include "glTFLightBase.h"

class glTFLocalLight : public glTFLightBase
{
public:
    glTFLocalLight(glTFLightType type, const glm::vec3& position = {0.0f, 0.0f, 0.0f});

    void SetPosition(const glm::vec3& position);
    const glm::vec3& GetPosition() const;
    
private:
    glm::vec3 m_position;
};
