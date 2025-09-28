// DX use [0, 1] as depth clip range
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "RendererSceneCamera.h"
#include <glm/glm/gtx/euler_angles.hpp>

RendererSceneCamera::RendererSceneCamera(glm::fvec3 camera_local_position, float fov_angle, float projection_width, float projection_height,
    float projection_near, float projection_far, CameraMode mode)
        : m_mode(mode)
        , m_projection_fov_radian(fov_angle)
        , m_projection_width(projection_width)
        , m_projection_height(projection_height)
        , m_projection_near(projection_near)
        , m_projection_far(projection_far)
        , m_camera_local_position(camera_local_position)
        , m_observe_center({})
        , m_observe_matrix(glm::fmat4x4{1.0f})
{
    
}

glm::fmat4x4 RendererSceneCamera::GetViewProjectionMatrix() const
{
    return GetViewMatrix() * GetProjectionMatrix();
}

glm::fmat4x4 RendererSceneCamera::GetViewMatrix() const
{
    glm::fmat4x4 view_matrix = m_observe_matrix;
    if (m_mode == CameraMode::Free)
    {
        view_matrix = glm::translate(glm::fmat4x4{1.0f}, m_camera_local_position);
    }
    
    return view_matrix;
}

glm::fmat4x4 RendererSceneCamera::GetProjectionMatrix() const
{
    return glm::perspectiveFovLH(m_projection_fov_radian, m_projection_width, m_projection_height, m_projection_near, m_projection_far);
}

void RendererSceneCamera::SetCameraMode(CameraMode mode)
{
    m_mode = mode;
}

const RendererSceneCamera::CameraMode& RendererSceneCamera::GetCameraMode() const
{
    return m_mode;
}

void RendererSceneCamera::Observe(const glm::vec3& center)
{
    SetCameraMode(CameraMode::Observer);
    m_observe_center = center;
}

void RendererSceneCamera::ObserveRotateXY(float rotation_x, float rotation_y)
{
    glm::vec4 distance_in_viewspace = m_observe_matrix * glm::fvec4{GetCameraPosition() - GetObserveCenter(), 0.0f};
    
    const glm::mat4 observe_inverse_matrix = glm::inverse(m_observe_matrix); 

    const glm::mat4 rotation_matrix = glm::eulerAngleXYZ(rotation_x, rotation_y, 0.0f);
    distance_in_viewspace = observe_inverse_matrix * rotation_matrix * distance_in_viewspace;
    const glm::vec3 new_observe_position = GetObserveCenter() + glm::vec3(distance_in_viewspace);
    SetCameraPosition(new_observe_position);
    
    m_observe_matrix = glm::lookAtLH(GetCameraPosition(), m_observe_center, {0.0f, 1.0f, 0.0f});
}

float RendererSceneCamera::GetObserveDistance() const
{
    return glm::length(m_observe_center - GetCameraPosition());
}

const glm::vec3& RendererSceneCamera::GetObserveCenter() const
{
    return m_observe_center;
}

void RendererSceneCamera::SetCameraPosition(const glm::fvec3& position)
{
    m_camera_local_position = position;
}

glm::vec3 RendererSceneCamera::GetCameraPosition() const
{
    return m_camera_local_position;
}

void RendererSceneCamera::GetCameraViewportSize(unsigned& out_width, unsigned& out_height) const
{
    out_width = static_cast<unsigned>(m_projection_width);
    out_height = static_cast<unsigned>(m_projection_height);
}