#pragma once

#include <string>
#include <glm/glm/gtx/compatibility.hpp>

#include "RendererCommon.h"
#include "glTFRHI/RHIInterface/IRHIMemoryManager.h"
#include "glTFRHI/RHIInterface/RHICommon.h"

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

enum class RenderPassResourceTableId
{
    // Common
    Depth,
    ScreenUVOffset,
    RayTracingSceneOutput,
    
    // Pass specific
    BasePass_Albedo,
    BasePass_Normal,
    LightingPass_Output,
    RayTracingPass_ReSTIRSample_Output,
    ComputePass_RayTracingOutputPostProcess_Output,

    // Custom for test pass
    TestPass_Custom0,
    TestPass_Custom1,
    TestPass_Custom2,
    TestPass_Custom3,
};

// Declare resource export/import info for specific render pass
struct RenderPassResourceTable
{
    std::vector<std::pair<RHITextureDesc, RenderPassResourceTableId>> m_export_texture_table_entry;
    std::vector<std::pair<RHITextureDesc, RenderPassResourceTableId>> m_import_texture_table_entry;
};

struct RenderPassResourceLocation
{
    std::map<RenderPassResourceTableId, std::shared_ptr<IRHITexture>> m_texture_allocations;
};
