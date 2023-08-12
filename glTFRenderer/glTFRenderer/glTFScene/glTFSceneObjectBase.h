#pragma once

#include <common.hpp>
#include <common.hpp>
#include <common.hpp>
#include <common.hpp>
#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/matrix_transform.hpp>
#include <glm/glm/gtx/euler_angles.hpp>
#include <glm/glm/gtx/matrix_decompose.hpp>

#include "glTFAABB.h"
#include "../glTFUtils/glTFUtils.h"
#include "../glTFLoader/glTFElementCommon.h"

struct glTF_Transform_WithTRS : public glTF_Transform
{
    glTF_Transform_WithTRS(const glm::fmat4& matrix = glm::mat4(1.0f));

    glTF_Transform_WithTRS(const glTF_Transform_WithTRS& rhs);

    glTF_Transform_WithTRS& operator=(const glTF_Transform_WithTRS& rhs);

    ~glTF_Transform_WithTRS() = default;
    
    static const glTF_Transform_WithTRS identity;

    void Translate(const glm::fvec3& translation);
    void TranslateOffset(const glm::fvec3& translation);
    void RotateEulerAngle(const glm::fvec3& euler_angle);
    void RotateEulerAngleOffset(const glm::fvec3& euler_angle);
    void Scale(const glm::fvec3& scale);
    
    void Update() const;
    
    glm::fmat4 GetTransformMatrix() const;

    glm::fmat4x4 GetTransformInverseMatrix() const;

    const glm::vec3& GetTranslation() const;

    static bool DecomposeMatrix(const glm::mat4& matrix, glm::vec3& out_translation, glm::quat& out_rotation, glm::vec3& out_scale);

    static glm::fvec3 GetTranslationFromMatrix(const glm::mat4& matrix);

    void MarkDirty() const;
    
protected:
    void UpdateEulerAngleToQuat();
    
    mutable bool m_dirty;
    
    glm::fvec3 m_translation;
    glm::fvec3 m_euler_angles;
    glm::quat m_quat;
    glm::fvec3 m_scale;
};

// Base class represent transform object in scene
class glTFSceneObjectBase : public glTFUniqueObject, public ITickable
{
public:
    glTFSceneObjectBase(const glTF_Transform_WithTRS& parentTransformRef)
        : m_parent_final_transform(parentTransformRef)
        , m_transform(glTF_Transform_WithTRS::identity)
        , m_dirty(true)
        , m_visible(true)
    {
        
    }

    void Translate(const glm::fvec3& translation_delta);
    void TranslateOffset(const glm::fvec3& translation_delta);
    void Rotate(const glm::fvec3& rotation_delta);
    void RotateOffset(const glm::fvec3& rotation_delta);
    void Scale(const glm::fvec3& scale);

    glm::mat4 GetTransformMatrix() const {return m_parent_final_transform.GetTransformMatrix() *  m_transform.GetTransformMatrix(); }
    glm::mat4 GetTransformInverseMatrix() const {return inverse(GetTransformMatrix()); }

    void SetAABB(const glTF_AABB::AABB& AABB);
    const glTF_AABB::AABB& GetAABB() const;

    void MarkDirty();
    bool IsDirty() const;
    void ResetDirty();
    
    bool IsVisible() const {return m_visible; }
    void SetVisible(bool visible) {m_visible = visible;}
    
protected:
    const glTF_Transform_WithTRS& m_parent_final_transform;
    glTF_Transform_WithTRS m_transform;

    glTF_AABB::AABB m_AABB;
    bool m_dirty;
    
    bool m_visible;
};
