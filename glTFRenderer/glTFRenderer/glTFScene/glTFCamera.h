#pragma once
#include "glTFSceneObjectBase.h"

enum class CameraMode
{
    Observer,
    Free
};

// This class represent abstract camera for viewport, also handle projection view
class glTFCamera : public glTFSceneObjectBase
{
public:
    glTFCamera(const glTF_Transform_WithTRS& parentTransformRef, float fovAngle, float projectionWidth, float projectionHeight, float projectionNear, float projectionFar)
        : glTFSceneObjectBase(parentTransformRef)
        , m_mode(CameraMode::Free)
        , m_projection_fov_radian(fovAngle * glm::pi<float>() / 180.0f)
        , m_projection_width(projectionWidth)
        , m_projection_height(projectionHeight)
        , m_projection_near(projectionNear)
        , m_projection_far(projectionFar)
        , m_observe_center({0.0f, 0.0f, 0.0f})
        , m_observe_matrix({1.0f})
    {}

    glm::fmat4x4 GetViewProjectionMatrix() const;
    glm::fmat4x4 GetViewMatrix() const;
    glm::fmat4x4 GetProjectionMatrix() const;

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

    glm::vec3 m_observe_center;
    glm::mat4 m_observe_matrix;
};