#include "glTFSceneObjectBase.h"

#include <gtx/quaternion.hpp>

#include "../glTFRHI/RHIDX12Impl/DX12CommandAllocator.h"
#include "../glTFUtils/glTFLog.h"

glTFUniqueID glTFUniqueObject::_innerUniqueID = 0;

const glTF_Transform_WithTRS glTF_Transform_WithTRS::identity;

void glTFSceneObjectBase::Translate(const glm::fvec3& translation_delta)
{
    m_transform.m_matrix = glm::translate(m_transform.m_matrix, translation_delta);
    /*
    glm::vec3 translation, scale;
    glm::quat rotation;
    glTF_Transform_WithTRS::DecomposeMatrix(m_transform.m_matrix, translation, rotation, scale);
    const glm::vec3 new_translation = translation + translation_delta;
    ResetTransform(new_translation, glm::eulerAngles(rotation), scale);
    */
    MarkDirty();
}

void glTFSceneObjectBase::Rotate(const glm::fvec3& rotation_delta)
{
    m_transform.m_matrix *= glm::eulerAngleXYZ(rotation_delta.x, rotation_delta.y, rotation_delta.z);
    /*
    glm::vec3 translation, scale;
    glm::quat rotation;
    glTF_Transform_WithTRS::DecomposeMatrix(m_transform.m_matrix, translation, rotation, scale);
    const glm::vec3 new_rotation = eulerAngles(rotation) + rotation_delta;
    ResetTransform(translation, new_rotation, scale);
    */
    MarkDirty();
}

void glTFSceneObjectBase::Scale(const glm::fvec3& scale)
{
    // TODO:

    MarkDirty();
}

const glTF_Transform_WithTRS& glTFSceneObjectBase::GetTransform() const
{
    return m_transform;
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

void glTFSceneObjectBase::ResetTransform(const glm::vec3& translation, const glm::vec3& euler, const glm::vec3& scale)
{
    glm::quat rot = glm::toQuat(glm::orientate3(euler));
    glm::mat4 rot_matrix = glm::toMat4(rot);
    glm::mat4 translate_matrix = glm::translate(glm::mat4(1.0f), translation);
    m_transform.m_matrix = rot_matrix * translate_matrix;
    
    glm::vec3 new_translation, new_scale;
    glm::quat new_euler;
    glTF_Transform_WithTRS::DecomposeMatrix(m_transform.m_matrix, new_translation, new_euler, new_scale);
    glm::vec3 new_rotation = glm::eulerAngles(new_euler);
    
    LOG_FORMAT("[INFO] Translate result %f %f %f old %f %f %f\n rotation %f %f %f old %f %f %f\n", new_translation.x, new_translation.y, new_translation.z,
        translation.x, translation.y, translation.z, euler.x, euler.y, euler.z, new_rotation.x, new_rotation.y, new_rotation.z);
}
