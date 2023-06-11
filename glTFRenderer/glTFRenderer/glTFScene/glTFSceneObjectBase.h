#pragma once

#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/matrix_transform.hpp>
#include <glm/glm/gtx/euler_angles.hpp>
#include <glm/glm/gtx/matrix_decompose.hpp>

#include "../glTFUtils/glTFUtils.h"
#include "../glTFLoader/glTFElementCommon.h"

struct glTF_Transform_WithTRS : glTF_Transform
{
    glTF_Transform_WithTRS(
        const glm::fmat4& baseTransform = glm::fmat4(1.0f),
        const glm::fvec3& translation = {0.0f, 0.0f, 0.0f},
        const glm::fvec3& rotation = {0.0f, 0.0f, 0.0f},
        const glm::fvec3& scale = {1.0f, 1.0f, 1.0f}
       )
        : glTF_Transform(baseTransform)
        , m_translation(translation)
        , m_rotation(rotation)
        , m_scale(scale)
    {
    }

    static const glTF_Transform_WithTRS identity;
    
    glm::fvec3 m_translation;

    // eulerAngle
    glm::fvec3 m_rotation;
    
    glm::fvec3 m_scale;
    
    glm::fmat4 GetTransformMatrix() const
    {
        const glm::mat4 scaleTransform = glm::scale(glm::mat4(1.0f), {m_scale.x, m_scale.y, m_scale.z});
        const glm::mat4 rotateTransform = glm::eulerAngleXYZ(m_rotation.x, m_rotation.y, m_rotation.z);
        const glm::mat4 translateTransform = glm::translate(glm::mat4(1.0f), m_translation);

        const glm::mat4 TRSMat = translateTransform * scaleTransform * rotateTransform; 

        return TRSMat * m_matrix;
    }

    glm::fmat4x4 GetTransformInverseMatrix() const
    {
        return inverse(GetTransformMatrix());
    }

    static glm::fvec3 GetTranslationFromMatrix(const glm::mat4& matrix)
    {
        glm::vec3 scale;
        glm::quat rotation;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(matrix, scale, rotation, translation, skew,perspective);
        return translation;
    }
};

// Base class represent transform object in scene
class glTFSceneObjectBase : public glTFUniqueObject, public ITickable
{
public:
    glTFSceneObjectBase(const glTF_Transform_WithTRS& parentTransformRef)
        : m_parentTransform(parentTransformRef)
        , m_transform(glTF_Transform_WithTRS::identity)
    {
        
    }

    void Translate(const glm::fvec3& translation);
    void Rotate(const glm::fvec3& rotation);
    void Scale(const glm::fvec3& scale);
    
    glm::mat4 GetTransformMatrix() const {return m_parentTransform.GetTransformMatrix() * m_transform.GetTransformMatrix(); }
    glm::mat4 GetTransformInverseMatrix() const {return inverse(GetTransformMatrix()); }

protected:
    const glTF_Transform_WithTRS& m_parentTransform;
    glTF_Transform_WithTRS m_transform;
};
