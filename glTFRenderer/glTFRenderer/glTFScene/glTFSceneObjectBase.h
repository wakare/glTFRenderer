#pragma once

#include <functional>
#include "../glTFUtils/glTFUtils.h"
#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/matrix_transform.hpp>
#include <glm/glm/gtx/euler_angles.hpp>
#include <utility>

struct glTFTransform
{
    glm::fvec3 position;

    // eulerAngle
    glm::fvec3 rotation;
    
    glm::fvec3 scale;
    
    static glTFTransform Identity()
    {
        const glTFTransform transform = {
            {0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 0.0f},
            {1.0f, 1.0f, 1.0f}
        };
        return transform;
    }

    glm::fmat4 GetTransformMatrix() const
    {
        const glm::mat4 scaleTransform = glm::scale(glm::mat4(1.0f), {scale.x, scale.y, scale.z});
        const glm::mat4 rotateTransform = glm::eulerAngleXYZ(rotation.x, rotation.y, rotation.z);
        const glm::mat4 translateTransform = glm::translate(glm::mat4(1.0f), position);
        
        return translateTransform * scaleTransform * rotateTransform;
    }

    glm::fmat4x4 GetTransformInverseMatrix() const
    {
        return inverse(GetTransformMatrix());
    }
};

// Base class represent transform object in scene
class glTFSceneObjectBase : public glTFUniqueObject, public ITickable
{
public:
    ~glTFSceneObjectBase() override;

    glTFSceneObjectBase()
        : m_transform(glTFTransform::Identity())
    {
        
    }
    
    const glTFTransform& GetTransform() const { return m_transform; }
    glTFTransform& GetTransform() {return m_transform; }

    glm::mat4x4 GetTransformMatrix() const {return GetTransform().GetTransformMatrix(); }
    glm::mat4x4 GetTransformInverseMatrix() const {return GetTransform().GetTransformInverseMatrix(); }
    
protected:
    glTFTransform m_transform;
};
