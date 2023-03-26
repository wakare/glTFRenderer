// DX use [0, 1] as depth clip range
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "glTFCamera.h"

void glTFCamera::UpdateViewProjectionMatrix()
{
    assert(m_dirty);

    const glm::fmat4x4 cameraInverseTransform = GetTransformInverseMatrix();

    const glm::fmat4x4 perspectiveMatrix = glm::perspectiveFovLH(m_projectionFovRadian, m_projectionWidth, m_projectionHeight, m_projectionNear, m_projectionFar);
    m_cacheViewProjectionMatrix = perspectiveMatrix * cameraInverseTransform;
    
    m_dirty = false;
}
