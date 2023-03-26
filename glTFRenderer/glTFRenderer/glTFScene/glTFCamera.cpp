// DX use [0, 1] as depth clip range
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "glTFCamera.h"

#include <cassert>

void glTFCamera::UpdateViewProjectionMatrix() const
{
    assert(m_dirty);

    // Camera transform is nor equivalent to camera inverse transform matrix because the order of camera translation and rotation 
    //const glm::fmat4x4 cameraInverseTransform = GetTransformInverseMatrix();
    const glm::mat4 cameraTranslationMatrix = glm::translate(glm::mat4(1.0f), -m_transform.position);

    const glm::mat4 cameraRotationMatrix = glm::eulerAngleXYZ(-m_transform.rotation.x, -m_transform.rotation.y, -m_transform.rotation.z);

    const glm::fmat4x4 perspectiveMatrix = glm::perspectiveFovLH(m_projectionFovRadian, m_projectionWidth, m_projectionHeight, m_projectionNear, m_projectionFar);
    m_cacheViewProjectionMatrix = perspectiveMatrix * cameraRotationMatrix * cameraTranslationMatrix;
    
    m_dirty = false;
}
