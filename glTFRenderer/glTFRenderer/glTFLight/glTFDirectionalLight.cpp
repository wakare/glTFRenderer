#include "glTFDirectionalLight.h"

glTFDirectionalLight::glTFDirectionalLight(const glTF_Transform_WithTRS& parentTransformRef)
    : glTFLightBase(parentTransformRef, glTFLightType::DirectionalLight)
{
    
}

bool glTFDirectionalLight::IsLocal() const
{
    return false;
}

bool glTFDirectionalLight::AffectPosition(const glm::vec3& position) const
{
    return true;
}

glm::vec3 glTFDirectionalLight::GetDirection() const
{
    glm::vec4 local = {0.0f, 0.0f, 1.0f, 0.0f};
    local = GetTransformMatrix() * local;
    return glm::vec3(local);
}

LightShadowmapViewInfo glTFDirectionalLight::GetShadowmapViewInfo(const glTF_AABB::AABB& scene_bounds) const
{
    LightShadowmapViewInfo shadowmap_view_info;
    auto scene_bounds_min = scene_bounds.getMin();
    auto scene_bounds_max = scene_bounds.getMax();

    auto light_location = glTF_Transform_WithTRS::GetTranslationFromMatrix(GetTransformMatrix());
    shadowmap_view_info.view_matrix = glm::lookAtLH(light_location, light_location + GetDirection(), {0.0f, 1.0f, 0.0f});

    shadowmap_view_info.position = glm::vec4(light_location, 1.0f);
        
    glm::vec3 light_ndc_min{ FLT_MAX, FLT_MAX, FLT_MAX };
    glm::vec3 light_ndc_max{ FLT_MIN, FLT_MIN, FLT_MIN };

    glm::vec4 scene_corner[8] =
        {
        {scene_bounds_min.x, scene_bounds_min.y, scene_bounds_min.z, 1.0f},
        {scene_bounds_max.x, scene_bounds_min.y, scene_bounds_min.z, 1.0f},
        {scene_bounds_min.x, scene_bounds_max.y, scene_bounds_min.z, 1.0f},
        {scene_bounds_max.x, scene_bounds_max.y, scene_bounds_min.z, 1.0f},
        {scene_bounds_min.x, scene_bounds_min.y, scene_bounds_max.z, 1.0f},
        {scene_bounds_max.x, scene_bounds_min.y, scene_bounds_max.z, 1.0f},
        {scene_bounds_min.x, scene_bounds_max.y, scene_bounds_max.z, 1.0f},
        {scene_bounds_max.x, scene_bounds_max.y, scene_bounds_max.z, 1.0f},
    };

    for (unsigned i = 0; i < 8; ++i)
    {
        glm::vec4 ndc_to_light_view = shadowmap_view_info.view_matrix * scene_corner[i];

        light_ndc_min.x = (light_ndc_min.x > ndc_to_light_view.x) ? ndc_to_light_view.x : light_ndc_min.x;
        light_ndc_min.y = (light_ndc_min.y > ndc_to_light_view.y) ? ndc_to_light_view.y : light_ndc_min.y;
        light_ndc_min.z = (light_ndc_min.z > ndc_to_light_view.z) ? ndc_to_light_view.z : light_ndc_min.z;

        light_ndc_max.x = (light_ndc_max.x < ndc_to_light_view.x) ? ndc_to_light_view.x : light_ndc_max.x;
        light_ndc_max.y = (light_ndc_max.y < ndc_to_light_view.y) ? ndc_to_light_view.y : light_ndc_max.y;
        light_ndc_max.z = (light_ndc_max.z < ndc_to_light_view.z) ? ndc_to_light_view.z : light_ndc_max.z;
    }
        
    shadowmap_view_info.projection_matrix = glm::orthoLH(light_ndc_min.x, light_ndc_max.x, light_ndc_min.y, light_ndc_max.y, light_ndc_min.z, light_ndc_max.z);
    return shadowmap_view_info;
}