#pragma once
#include <vec3.hpp>
#include "glTFScene/glTFSceneObjectBase.h"

enum class glTFLightType
{
    PointLight,
    DirectionalLight,
    SpotLight,
    SkyLight,
    
};

struct LightShadowmapViewInfo
{
    glm::vec4 position;
    glm::mat4 view_matrix;
    glm::mat4 projection_matrix;
};

struct LightShadowmapViewRange
{
    float ndc_min_x;
    float ndc_min_y;
    float ndc_width;
    float ndc_height;
};

class glTFLightBase : public glTFSceneObjectBase
{
public:
    glTFLightBase(const glTF_Transform_WithTRS& parentTransformRef, glTFLightType type);

    glTFLightType GetType() const;
    virtual bool IsLocal() const = 0;

    virtual bool AffectPosition(const glm::vec3& position) const = 0;
    void SetIntensity(float intensity);
    float GetIntensity() const;

    void SetColor(const glm::vec3& color);
    const glm::vec3& GetColor() const;

    virtual LightShadowmapViewInfo GetShadowmapViewInfo(const glTF_AABB::AABB& scene_bounds, const LightShadowmapViewRange& range) const;
    
protected:
    glTFLightType m_type;
    glm::vec3 m_color;
    float m_intensity;
};
