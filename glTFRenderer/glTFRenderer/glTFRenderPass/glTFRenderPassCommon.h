#pragma once

#include <glm/glm/gtx/compatibility.hpp>

struct PointLightInfo
{
    glm::float4 position_and_radius;
    glm::float4 intensity_and_falloff;
};

struct DirectionalLightInfo
{
    glm::float4 directional_and_intensity;
};

struct ConstantBufferPerLightDraw
{
    unsigned point_light_count;
    unsigned directional_light_count;

    std::vector<PointLightInfo> point_light_infos;
    std::vector<DirectionalLightInfo> directional_light_infos;
};
