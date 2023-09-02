#pragma once
#include "glTFComputePassBase.h"
#include "glTFRenderPassCommon.h"

class glTFComputePassLighting : public glTFComputePassBase
{
public:

protected:
    std::map<glTFHandle::HandleIndexType, PointLightInfo> m_cache_point_lights;
    std::map<glTFHandle::HandleIndexType, DirectionalLightInfo> m_cache_directional_lights;
    ConstantBufferPerLightDraw m_constant_buffer_per_light_draw;
};
