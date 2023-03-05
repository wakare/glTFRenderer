#pragma once

#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/matrix_transform.hpp>
#include <glm/glm/gtx/euler_angles.hpp>

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

    glm::fmat4x4 GetTransformMatrix() const
    {
        glm::fmat4x4 result = glm::scale(glm::eulerAngleXYZ(rotation.x, rotation.y, rotation.z), {scale.x, scale.y, scale.z});
        result = glm::translate(result, {position.x, position.y, position.z});
        return result;
    }
};

// Base class represent transform object in scene
class glTFSceneObjectBase
{
public:
    virtual ~glTFSceneObjectBase() = default;

    glTFSceneObjectBase()
        : m_transform(glTFTransform::Identity())
    {
        
    }
    
    const glTFTransform& GetTransform() const { return m_transform; }
    glTFTransform& GetTransform() {return m_transform; }

    glm::mat4x4 GetTransformMatrix() const {return GetTransform().GetTransformMatrix(); }
    
protected:
    glTFTransform m_transform;
};
