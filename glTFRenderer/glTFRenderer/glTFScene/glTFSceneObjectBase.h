#pragma once

#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/matrix_transform.hpp>
#include <glm/glm/gtx/euler_angles.hpp>
#include <glm/glm/gtx/matrix_decompose.hpp>

#include "glTFAABB.h"
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
    {
    }

    static const glTF_Transform_WithTRS identity;
    
    glm::fmat4 GetTransformMatrix() const
    {
        return m_matrix;
    }

    glm::fmat4x4 GetTransformInverseMatrix() const
    {
        return inverse(GetTransformMatrix());
    }

    glm::vec3 GetTranslation() const
    {
        return GetTranslationFromMatrix(m_matrix);
    }

    static bool DecomposeMatrix(const glm::mat4& matrix, glm::vec3& out_translation, glm::quat& out_rotation, glm::vec3& out_scale)
    {
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(matrix, out_scale, out_rotation, out_translation, skew,perspective);
        return true;
    }
    
    static glm::fvec3 GetTranslationFromMatrix(const glm::mat4& matrix)
    {
        glm::vec3 scale;
        glm::quat rotation;
        glm::vec3 translation;
        
        const bool decomposed = DecomposeMatrix(matrix, translation, rotation, scale);
        GLTF_CHECK(decomposed);
        
        return translation;
    }
};

// Base class represent transform object in scene
class glTFSceneObjectBase : public glTFUniqueObject, public ITickable
{
public:
    glTFSceneObjectBase(const glTF_Transform_WithTRS& parentTransformRef)
        : m_parent_final_transform(parentTransformRef)
        , m_transform(glTF_Transform_WithTRS::identity)
        , m_dirty(true)
    {
        
    }

    void Translate(const glm::fvec3& translation_delta);
    void Rotate(const glm::fvec3& rotation_delta);
    void Scale(const glm::fvec3& scale);
    
    const glTF_Transform_WithTRS& GetTransform() const;
    
    glm::mat4 GetTransformMatrix() const {return m_parent_final_transform.GetTransformMatrix() *  m_transform.GetTransformMatrix(); }
    glm::mat4 GetTransformInverseMatrix() const {return inverse(GetTransformMatrix()); }

    void SetAABB(const glTF_AABB::AABB& AABB);
    const glTF_AABB::AABB& GetAABB() const;

    void MarkDirty();
    bool IsDirty() const;
    void ResetDirty();
    
protected:
    void ResetTransform(const glm::vec3& translation, const glm::vec3& euler, const glm::vec3& scale);
    
    const glTF_Transform_WithTRS& m_parent_final_transform;
    glTF_Transform_WithTRS m_transform;

    glTF_AABB::AABB m_AABB;
    bool m_dirty;
};
