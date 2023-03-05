#include "glTFCamera.h"

// DX use [0, 1] as depth clip range
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

void glTFCamera::UpdateViewProjectionMatrix()
{
    assert(m_dirty);

    glm::fmat4x4 viewMatrix = glm::identity<glm::fmat4x4>();
    viewMatrix = glm::translate(viewMatrix, -m_transform.position);

    const glm::fmat4x4 rotationMatrix = glm::eulerAngleXYZ(m_transform.rotation.x, m_transform.rotation.y, m_transform.rotation.z);
    viewMatrix *= rotationMatrix;

    const glm::fmat4x4 perspectiveMatrix = glm::perspectiveFovLH(m_projectionFovRadian, m_projectionWidth, m_projectionHeight, m_projectionNear, m_projectionFar);
    m_cacheViewProjectionMatrix = viewMatrix * perspectiveMatrix;
    
    m_dirty = true;
}
