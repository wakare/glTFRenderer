// DX use [0, 1] as depth clip range
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "glTFCamera.h"

#include "../glTFUtils/glTFLog.h"

glm::fmat4x4 glTFCamera::GetViewProjectionMatrix() const
{
    return GetViewMatrix() * GetProjectionMatrix();
}

glm::fmat4x4 glTFCamera::GetViewMatrix() const
{
    return m_transform.m_matrix;
}

glm::fmat4x4 glTFCamera::GetProjectionMatrix() const
{
    return glm::perspectiveFovLH(m_projection_fov_radian, m_projection_width, m_projection_height, m_projection_near, m_projection_far);
}

void glTFCamera::MarkDirty() const
{
    m_dirty = true;
}

void glTFCamera::SetCameraMode(CameraMode mode)
{
    m_mode = mode;
}

const CameraMode& glTFCamera::GetCameraMode() const
{
    return m_mode;
}

void glTFCamera::Observe(const glm::vec3& center, float distance)
{
    SetCameraMode(CameraMode::Observer);
    m_observe_center = center;
    m_observe_distance = distance;
}

void glTFCamera::LookAtObserve(const glm::vec3& position)
{
    const glm::mat4 final_matrix = glm::lookAtLH(position, m_observe_center, {0.0f, 1.0f, 0.0f})
    * glm::inverse(m_parent_final_transform.m_matrix);
    m_transform.m_matrix = final_matrix;

    LOG_FORMAT("[INFO] New position %f %f %f \n", position.x, position.y, position.z);
}

const glm::vec3& glTFCamera::GetObserveCenter() const
{
    return m_observe_center;
}

float glTFCamera::GetObserveDistance() const
{
    return m_observe_distance;
}