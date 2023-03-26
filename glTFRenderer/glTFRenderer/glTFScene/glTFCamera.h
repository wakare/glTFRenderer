#pragma once
#include "glTFSceneObjectBase.h"

// This class represent abstract camera for viewport, also handle projection view
class glTFCamera : public glTFSceneObjectBase
{
public:
    glTFCamera(float fovAngle, float projectionWidth, float projectionHeight, float projectionNear, float projectionFar)
        : m_dirty(true)
        , m_projectionFovRadian(fovAngle * glm::pi<float>() / 180.0f)
        , m_projectionWidth(projectionWidth)
        , m_projectionHeight(projectionHeight)
        , m_projectionNear(projectionNear)
        , m_projectionFar(projectionFar)
        // glm::fmat4x4(1.0f) represent identity matrix
        , m_cacheViewProjectionMatrix(glm::fmat4x4(1.0f))
    {}

    const glm::fmat4x4& GetViewProjectionMatrix() const
    {
        if (m_dirty)
        {
            UpdateViewProjectionMatrix();
        }
        return m_cacheViewProjectionMatrix;
    } 

    void MarkDirty() { m_dirty = true; }
    
protected:
    void UpdateViewProjectionMatrix() const;
    
    mutable bool m_dirty;

    float m_projectionFovRadian;
    float m_projectionWidth;
    float m_projectionHeight;
    float m_projectionNear;
    float m_projectionFar;

    mutable glm::fmat4x4 m_cacheViewProjectionMatrix;
};