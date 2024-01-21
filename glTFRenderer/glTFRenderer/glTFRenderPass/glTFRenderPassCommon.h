#pragma once

#include <glm/glm/gtx/compatibility.hpp>

struct LightInfoConstant
{
    unsigned light_count {0};
};

ALIGN_FOR_CBV_STRUCT struct LightInfo
{
    inline static std::string Name = "SCENE_LIGHT_INFO_REGISTER_INDEX";
    
    glm::float3 position;
    float radius;
    
    glm::float3 intensity;
    unsigned type;
};

ALIGN_FOR_CBV_STRUCT struct ConstantBufferPerLightDraw
{
    inline static std::string Name = "SCENE_LIGHT_INFO_CONSTANT_REGISTER_INDEX";
    
    LightInfoConstant light_info;
};
