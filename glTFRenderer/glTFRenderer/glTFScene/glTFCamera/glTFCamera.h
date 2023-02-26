#pragma once
#include "../glTFSceneObjectBase.h"

// This class represent abstract camera for viewport, also handle projection view
class glTFCamera : public glTFSceneObjectBase
{
public:
    glTFCamera(float projectionWidth, float projectionHeight, float projectionNear, float projectionFar)
        : m_dirty(true)
        , m_projectionWidth(projectionWidth)
        , m_projectionHeight(projectionHeight)
        , m_projectionNear(projectionNear)
        , m_projectionFar(projectionFar)
        // glm::fmat4x4(1.0f) represent identity matrix
        , m_cacheViewProjectionMatrix(glm::fmat4x4(1.0f))
    {}

    const glm::fmat4x4& GetViewProjectionMatrix()
    {
        if (m_dirty)
        {
            m_dirty = false;
            UpdateViewProjectionMatrix();
        }
        return m_cacheViewProjectionMatrix;
    } 

    void MarkDirty() { m_dirty = true; }
    
protected:
    void UpdateViewProjectionMatrix();
    
    bool m_dirty; 
    
    float m_projectionWidth;
    float m_projectionHeight;
    float m_projectionNear;
    float m_projectionFar;

    glm::fmat4x4 m_cacheViewProjectionMatrix;
};
