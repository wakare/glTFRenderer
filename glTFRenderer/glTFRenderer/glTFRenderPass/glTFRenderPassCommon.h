#pragma once

#include <map>
#include <glm/glm/gtx/compatibility.hpp>

struct LightInfo
{
    unsigned point_light_count;
    unsigned directional_light_count;
};

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
    LightInfo light_info;

    std::vector<PointLightInfo> point_light_infos;
    std::vector<DirectionalLightInfo> directional_light_infos;
};
