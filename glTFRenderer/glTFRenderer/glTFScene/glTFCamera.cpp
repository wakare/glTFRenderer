// DX use [0, 1] as depth clip range
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "glTFCamera.h"

#include <cassert>


const glm::fmat4x4& glTFCamera::GetViewProjectionMatrix() const
{
    if (m_dirty)
    {
        UpdateViewProjectionMatrix();
    }
    return m_cacheViewProjectionMatrix;
}

const glm::fmat4x4& glTFCamera::GetViewMatrix() const
{
    return m_cacheViewMatrix;
}

const glm::fmat4x4& glTFCamera::GetProjectionMatrix() const
{
    return m_cacheProjectionMatrix;
}

void glTFCamera::UpdateViewProjectionMatrix() const
{
    assert(m_dirty);

    // Camera transform is nor equivalent to camera inverse transform matrix because the order of camera translation and rotation 
    //const glm::fmat4x4 cameraInverseTransform = GetTransformInverseMatrix();
    const glm::mat4 cameraTranslationMatrix = glm::translate(glm::mat4(1.0f), -m_transform.m_translation);
    const glm::mat4 cameraRotationMatrix = glm::eulerAngleXYZ(-m_transform.m_rotation.x, -m_transform.m_rotation.y, -m_transform.m_rotation.z);
    m_cacheViewMatrix = cameraRotationMatrix * cameraTranslationMatrix;
    
    m_cacheProjectionMatrix = glm::perspectiveFovLH(m_projectionFovRadian, m_projectionWidth, m_projectionHeight, m_projectionNear, m_projectionFar);
    m_cacheViewProjectionMatrix = m_cacheProjectionMatrix * m_cacheViewMatrix;
    
    m_dirty = false;
}
