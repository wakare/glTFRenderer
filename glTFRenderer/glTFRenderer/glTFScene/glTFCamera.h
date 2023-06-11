#pragma once
#include "glTFSceneObjectBase.h"

// This class represent abstract camera for viewport, also handle projection view
class glTFCamera : public glTFSceneObjectBase
{
public:
    glTFCamera(const glTF_Transform_WithTRS& parentTransformRef, float fovAngle, float projectionWidth, float projectionHeight, float projectionNear, float projectionFar)
        : glTFSceneObjectBase(parentTransformRef)
        , m_dirty(true)
        , m_projectionFovRadian(fovAngle * glm::pi<float>() / 180.0f)
        , m_projectionWidth(projectionWidth)
        , m_projectionHeight(projectionHeight)
        , m_projectionNear(projectionNear)
        , m_projectionFar(projectionFar)
        , m_cacheViewMatrix({1.0f})
        , m_cacheProjectionMatrix({1.0f})
        , m_cacheViewProjectionMatrix({1.0f})
    {}

    const glm::fmat4x4& GetViewProjectionMatrix() const;
    const glm::fmat4x4& GetViewMatrix() const;
    const glm::fmat4x4& GetProjectionMatrix() const;

    void MarkDirty() { m_dirty = true; }
    
protected:
    void UpdateViewProjectionMatrix() const;
    
    mutable bool m_dirty;

    float m_projectionFovRadian;
    float m_projectionWidth;
    float m_projectionHeight;
    float m_projectionNear;
    float m_projectionFar;

    mutable glm::fmat4x4 m_cacheViewMatrix;
    mutable glm::fmat4x4 m_cacheProjectionMatrix;
    mutable glm::fmat4x4 m_cacheViewProjectionMatrix;
};