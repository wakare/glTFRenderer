#include "glTFSceneObjectBase.h"

glTFUniqueID glTFUniqueObject::_innerUniqueID = 0;

const glTF_Transform_WithTRS glTF_Transform_WithTRS::identity;

void glTFSceneObjectBase::Translate(const glm::fvec3& translation)
{
    m_transform.m_translation += translation;
}

void glTFSceneObjectBase::Rotate(const glm::fvec3& rotation)
{
    m_transform.m_rotation += rotation;
}

void glTFSceneObjectBase::Scale(const glm::fvec3& scale)
{
    m_transform.m_scale += scale;
}
