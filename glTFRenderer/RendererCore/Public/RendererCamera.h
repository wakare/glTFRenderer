#pragma once
#include <memory>
#include <glm/glm/glm.hpp>

class RendererSceneNodeTransform;

enum class CameraMode
{
    Observer,
    Free
};

struct RendererCameraDesc
{
    glm::fmat4 transform;
    float fov_angle;
    float projection_width;
    float projection_height;
    float projection_near;
    float projection_far;
    CameraMode mode = CameraMode::Free;
};

class RendererCamera
{
public:
    RendererCamera(const RendererCameraDesc& camera_desc);

    glm::fmat4x4 GetViewProjectionMatrix();
    glm::fmat4x4 GetViewMatrix();
    glm::fmat4x4 GetProjectionMatrix() const;

    float GetAspect() const {return m_projection_width / m_projection_height; }
    float GetNearZPlane() const {return m_projection_near; }
    float GetFarZPlane() const {return m_projection_far; }

    float GetFovX() const {return 2.0f * glm::atan(glm::tan(GetFovY() * 0.5f) * GetAspect()); }
    float GetFovY() const {return m_projection_fov_radian; }
    
    void SetCameraMode(CameraMode mode);
    const CameraMode& GetCameraMode() const;

    void Observe(const glm::fvec3& center);
    void ObserveRotateXY(float rotation_x, float rotation_y);
    float GetObserveDistance() const;
    
    const glm::fvec3& GetObserveCenter() const;
    void SetCameraPosition(const glm::fvec3& position);
    glm::fvec3 GetCameraPosition() const;

    void GetCameraViewportSize(unsigned& out_width, unsigned& out_height) const;
    RendererSceneNodeTransform& GetCameraTransform();

    void TranslateOffset(const glm::fvec3& translation);
    void RotateEulerAngleOffset(const glm::fvec3& euler_angle);

    void MarkTransformDirty();
    bool IsTransformDirty() const;

    float GetProjectionWidth() const;
    float GetProjectionHeight() const;
    void SetProjectionSize(float width, float height);
    
protected:
    CameraMode m_mode;

    float m_projection_fov_radian;
    float m_projection_width;
    float m_projection_height;
    float m_projection_near;
    float m_projection_far;

    std::shared_ptr<RendererSceneNodeTransform> m_transform;
    //glm::fvec3 m_camera_local_position;
    glm::fvec3 m_observe_center;
    glm::fmat4 m_observe_matrix;
};
