#pragma once
#include <glm/glm/glm.hpp>

class RendererSceneCamera
{
public:
    enum class CameraMode
    {
        Observer,
        Free
    };

    RendererSceneCamera(glm::fvec3 camera_local_position, float fov_angle, float projection_width, float projection_height, float projection_near, float projection_far, CameraMode mode = CameraMode::Free);

    glm::fmat4x4 GetViewProjectionMatrix() const;
    glm::fmat4x4 GetViewMatrix() const;
    glm::fmat4x4 GetProjectionMatrix() const;

    float GetAspect() const {return m_projection_width / m_projection_height; }
    float GetNearZPlane() const {return m_projection_near; }
    float GetFarZPlane() const {return m_projection_far; }

    float GetFovX() const {return 2.0f * glm::atan(glm::tan(GetFovY() * 0.5f) * GetAspect()); }
    float GetFovY() const {return m_projection_fov_radian; }
    
    void SetCameraMode(CameraMode mode);
    const CameraMode& GetCameraMode() const;

    void Observe(const glm::vec3& center);
    void ObserveRotateXY(float rotation_x, float rotation_y);
    float GetObserveDistance() const;
    
    const glm::vec3& GetObserveCenter() const;
    void SetCameraPosition(const glm::fvec3& position);
    glm::vec3 GetCameraPosition() const;

    void GetCameraViewportSize(unsigned& out_width, unsigned& out_height) const;
    
protected:
    CameraMode m_mode;

    float m_projection_fov_radian;
    float m_projection_width;
    float m_projection_height;
    float m_projection_near;
    float m_projection_far;

    glm::fvec3 m_camera_local_position;
    glm::fvec3 m_observe_center;
    glm::fmat4 m_observe_matrix;
};