#include "glTFSceneObjectBase.h"

#include <gtx/quaternion.hpp>

glTFUniqueID glTFUniqueObject::_innerUniqueID = 0;

const glTF_Transform_WithTRS glTF_Transform_WithTRS::identity;

glTF_Transform_WithTRS::glTF_Transform_WithTRS(const glm::fmat4& matrix): glTF_Transform(matrix),
                                                                          m_dirty(true),
                                                                          m_translation(),
                                                                          m_euler_angles(),
                                                                          m_quat(),
                                                                          m_scale()
{
    // Init TRS
    DecomposeMatrix(matrix, m_translation, m_quat, m_scale);
}

glTF_Transform_WithTRS::glTF_Transform_WithTRS(const glTF_Transform_WithTRS& rhs): glTF_Transform(rhs.GetTransformMatrix()) ,
                                                                                m_dirty(true),
                                                                                m_translation(rhs.m_translation),
                                                                                m_euler_angles(rhs.m_euler_angles),
                                                                                m_quat(rhs.m_quat),
                                                                                m_scale(rhs.m_scale)
{
        
}

glTF_Transform_WithTRS& glTF_Transform_WithTRS::operator=(const glTF_Transform_WithTRS& rhs)
{
    m_matrix = rhs.m_matrix;
    m_dirty = true;
    m_translation = rhs.m_translation;
    m_quat = rhs.m_quat;
    m_scale = rhs.m_scale;
        
    return *this;
}

glm::fmat4 glTF_Transform_WithTRS::GetTransformMatrix() const
{
    if (m_dirty)
    {
        Update();
    }
        
    return m_matrix;
}

glm::fmat4x4 glTF_Transform_WithTRS::GetTransformInverseMatrix() const
{
    return inverse(GetTransformMatrix());
}

const glm::vec3& glTF_Transform_WithTRS::GetTranslation() const
{
    return m_translation;
}

bool glTF_Transform_WithTRS::DecomposeMatrix(const glm::mat4& matrix, glm::vec3& out_translation,
    glm::quat& out_rotation, glm::vec3& out_scale)
{
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(matrix, out_scale, out_rotation, out_translation, skew,perspective);
    return true;
}

glm::fvec3 glTF_Transform_WithTRS::GetTranslationFromMatrix(const glm::mat4& matrix)
{
    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 translation;
        
    const bool decomposed = DecomposeMatrix(matrix, translation, rotation, scale);
    GLTF_CHECK(decomposed);
        
    return translation;
}

void glTF_Transform_WithTRS::Translate(const glm::fvec3& translation)
{
    m_translation = translation;
    MarkDirty();
}

void glTF_Transform_WithTRS::TranslateOffset(const glm::fvec3& translation)
{
    m_translation += translation;
    MarkDirty();
}

void glTF_Transform_WithTRS::RotateEulerAngle(const glm::fvec3& euler_angle)
{
    m_euler_angles = euler_angle;
    UpdateEulerAngleToQuat();
}

void glTF_Transform_WithTRS::RotateEulerAngleOffset(const glm::fvec3& euler_angle)
{
    m_euler_angles += euler_angle;
    UpdateEulerAngleToQuat();
}

void glTF_Transform_WithTRS::Scale(const glm::fvec3& scale)
{
    m_scale = scale;
    MarkDirty();
}

void glTF_Transform_WithTRS::Update() const
{
    if (m_dirty)
    {
        const glm::mat4 rotation_matrix = glm::toMat4(m_quat);
        const glm::mat4 scale_matrix = glm::scale(glm::mat4(1.0f), m_scale);
        const glm::mat4 translation_matrix = glm::translate(glm::mat4(1.0f), m_translation);
        
        m_matrix = translation_matrix * rotation_matrix * scale_matrix;
        
        m_dirty = false;
    }
}

void glTF_Transform_WithTRS::MarkDirty() const
{
    m_dirty = true;
}

void glTF_Transform_WithTRS::UpdateEulerAngleToQuat()
{
    m_quat = m_euler_angles;
    MarkDirty();
}

void glTFSceneObjectBase::Translate(const glm::fvec3& translation_delta)
{
    m_transform.Translate(translation_delta);
    MarkDirty();
}

void glTFSceneObjectBase::TranslateOffset(const glm::fvec3& translation_delta)
{
    m_transform.TranslateOffset(translation_delta);
    MarkDirty();
}

void glTFSceneObjectBase::Rotate(const glm::fvec3& rotation_delta)
{
    m_transform.RotateEulerAngle(rotation_delta);
    MarkDirty();
}

void glTFSceneObjectBase::RotateOffset(const glm::fvec3& rotation_delta)
{
    m_transform.RotateEulerAngleOffset(rotation_delta);
    MarkDirty();
}

void glTFSceneObjectBase::Scale(const glm::fvec3& scale)
{
    m_transform.Scale(scale);
    MarkDirty();
}

void glTFSceneObjectBase::SetAABB(const glTF_AABB::AABB& AABB)
{
    m_AABB = AABB;
}

const glTF_AABB::AABB& glTFSceneObjectBase::GetAABB() const
{
    return m_AABB;
}

void glTFSceneObjectBase::MarkDirty()
{
    m_dirty = true;
}

bool glTFSceneObjectBase::IsDirty() const
{
    return m_dirty;
}

void glTFSceneObjectBase::ResetDirty()
{
    m_dirty = false;    
}