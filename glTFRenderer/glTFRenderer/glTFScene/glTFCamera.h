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
        , m_dirty(true)
        , m_projection_fov_radian(fovAngle * glm::pi<float>() / 180.0f)
        , m_projection_width(projectionWidth)
        , m_projection_height(projectionHeight)
        , m_projection_near(projectionNear)
        , m_projection_far(projectionFar)
        , m_observe_center({0.0f, 0.0f, 0.0f})
        , m_observe_distance(1.0f)
    {}

    glm::fmat4x4 GetViewProjectionMatrix() const;
    glm::fmat4x4 GetViewMatrix() const;
    glm::fmat4x4 GetProjectionMatrix() const;

    void MarkDirty() const;

    void SetCameraMode(CameraMode mode);
    const CameraMode& GetCameraMode() const;

    void Observe(const glm::vec3& center, float distance);
    void LookAtObserve(const glm::vec3& position);
    const glm::vec3& GetObserveCenter() const;
    float GetObserveDistance() const;
    
protected:
    void UpdateViewProjectionMatrix() const;

    CameraMode m_mode;
    
    mutable bool m_dirty;

    float m_projection_fov_radian;
    float m_projection_width;
    float m_projection_height;
    float m_projection_near;
    float m_projection_far;

    glm::vec3 m_observe_center;
    float m_observe_distance;
};