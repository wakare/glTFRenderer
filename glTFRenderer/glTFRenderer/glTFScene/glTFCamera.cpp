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
    return m_mode == CameraMode::Free ? m_transform.GetTransformMatrix() : m_observe_matrix;
}

glm::fmat4x4 glTFCamera::GetProjectionMatrix() const
{
    return glm::perspectiveFovLH(m_projection_fov_radian, m_projection_width, m_projection_height, m_projection_near, m_projection_far);
}

void glTFCamera::SetCameraMode(CameraMode mode)
{
    m_mode = mode;
}

const CameraMode& glTFCamera::GetCameraMode() const
{
    return m_mode;
}

void glTFCamera::Observe(const glm::vec3& center)
{
    SetCameraMode(CameraMode::Observer);
    m_observe_center = center;
}

void glTFCamera::ObserveRotateXY(float rotation_x, float rotation_y)
{
    glm::vec4 distance_in_viewspace = m_observe_matrix * glm::fvec4{GetCameraPosition() - GetObserveCenter(), 0.0f};
    
    const glm::mat4 observe_inverse_matrix = glm::inverse(m_observe_matrix); 

    const glm::mat4 rotation_matrix = glm::eulerAngleXYZ(rotation_x, rotation_y, 0.0f);
    distance_in_viewspace = observe_inverse_matrix * rotation_matrix * distance_in_viewspace;
    const glm::vec3 new_observe_position = GetObserveCenter() + glm::vec3(distance_in_viewspace);
    SetCameraPosition(new_observe_position);
    
    m_observe_matrix = glm::lookAtLH(GetCameraPosition(), m_observe_center, {0.0f, 1.0f, 0.0f});
}

float glTFCamera::GetObserveDistance() const
{
    return glm::length(m_observe_center - GetCameraPosition());
}

const glm::vec3& glTFCamera::GetObserveCenter() const
{
    return m_observe_center;
}

void glTFCamera::SetCameraPosition(const glm::fvec3& position)
{
    m_transform.Translate(position);
}

glm::vec3 glTFCamera::GetCameraPosition() const
{
    return m_transform.GetTranslation();
}
