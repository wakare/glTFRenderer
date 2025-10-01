// DX use [0, 1] as depth clip range
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "RendererCamera.h"
#include <glm/glm/gtx/euler_angles.hpp>
#include <utility>

#include "RendererSceneGraph.h"

RendererCamera::RendererCamera(const RendererCameraDesc& camera_desc)
        : m_mode(camera_desc.mode)
        , m_projection_fov_radian(camera_desc.fov_angle)
        , m_projection_width(camera_desc.projection_width)
        , m_projection_height(camera_desc.projection_height)
        , m_projection_near(camera_desc.projection_near)
        , m_projection_far(camera_desc.projection_far)
        , m_transform(std::make_shared<RendererSceneNodeTransform>(camera_desc.transform))
        , m_observe_center({})
        , m_observe_matrix(glm::fmat4x4{1.0f})
{
    
}

glm::fmat4x4 RendererCamera::GetViewProjectionMatrix()
{
    return GetViewMatrix() * GetProjectionMatrix();
}

glm::fmat4x4 RendererCamera::GetViewMatrix()
{
    glm::fmat4x4 view_matrix = m_observe_matrix;
    if (m_mode == CameraMode::Free)
    {
        view_matrix = glm::inverse(m_transform->GetTransformMatrix());
    }
    
    return view_matrix;
}

glm::fmat4x4 RendererCamera::GetProjectionMatrix() const
{
    return glm::perspectiveFovLH(m_projection_fov_radian, m_projection_width, m_projection_height, m_projection_near, m_projection_far);
}

void RendererCamera::SetCameraMode(CameraMode mode)
{
    m_mode = mode;
}

const CameraMode& RendererCamera::GetCameraMode() const
{
    return m_mode;
}

void RendererCamera::Observe(const glm::vec3& center)
{
    SetCameraMode(CameraMode::Observer);
    m_observe_center = center;
}

void RendererCamera::ObserveRotateXY(float rotation_x, float rotation_y)
{
    glm::vec4 distance_in_viewspace = m_observe_matrix * glm::fvec4{GetCameraPosition() - GetObserveCenter(), 0.0f};
    
    const glm::mat4 observe_inverse_matrix = glm::inverse(m_observe_matrix); 

    const glm::mat4 rotation_matrix = glm::eulerAngleXYZ(rotation_x, rotation_y, 0.0f);
    distance_in_viewspace = observe_inverse_matrix * rotation_matrix * distance_in_viewspace;
    const glm::vec3 new_observe_position = GetObserveCenter() + glm::vec3(distance_in_viewspace);
    SetCameraPosition(new_observe_position);
    
    m_observe_matrix = glm::lookAtLH(GetCameraPosition(), m_observe_center, {0.0f, 1.0f, 0.0f});
}

float RendererCamera::GetObserveDistance() const
{
    return glm::length(m_observe_center - GetCameraPosition());
}

const glm::vec3& RendererCamera::GetObserveCenter() const
{
    return m_observe_center;
}

void RendererCamera::SetCameraPosition(const glm::fvec3& position)
{
    m_transform->Translate(position);
}

glm::vec3 RendererCamera::GetCameraPosition() const
{
    return m_transform->GetTranslation();
}

void RendererCamera::GetCameraViewportSize(unsigned& out_width, unsigned& out_height) const
{
    out_width = static_cast<unsigned>(m_projection_width);
    out_height = static_cast<unsigned>(m_projection_height);
}

RendererSceneNodeTransform& RendererCamera::GetCameraTransform()
{
    return *m_transform;
}

void RendererCamera::TranslateOffset(const glm::fvec3& translation)
{
    m_transform->TranslateOffset(translation);
}

void RendererCamera::RotateEulerAngleOffset(const glm::fvec3& euler_angle)
{
    return m_transform->RotateEulerAngleOffset(euler_angle);
}

void RendererCamera::MarkTransformDirty()
{
    return m_transform->MarkTransformDirty();
}

bool RendererCamera::IsTransformDirty() const
{
    return m_transform->IsTransformDirty();
}

