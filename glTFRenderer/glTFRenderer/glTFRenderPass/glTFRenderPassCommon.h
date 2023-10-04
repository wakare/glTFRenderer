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
    inline static std::string Name = "SCENE_LIGHT_POINT_LIGHT_REGISTER_INDEX"; 
    
    glm::float4 position_and_radius;
    glm::float4 intensity_and_falloff;
};

struct DirectionalLightInfo
{
    inline static std::string Name = "SCENE_LIGHT_DIRECTIONAL_LIGHT_REGISTER_INDEX";
    
    glm::float4 directional_and_intensity;
};

struct ConstantBufferPerLightDraw
{
    inline static std::string Name = "SCENE_LIGHT_LIGHT_INFO_REGISTER_INDEX";
    LightInfo light_info;

    std::vector<PointLightInfo> point_light_infos;
    std::vector<DirectionalLightInfo> directional_light_infos;
};
