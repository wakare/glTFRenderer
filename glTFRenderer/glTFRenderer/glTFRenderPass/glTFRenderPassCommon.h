#pragma once

#include <string>
#include <glm/glm/gtx/compatibility.hpp>

#include "RendererCommon.h"
#include "glTFRHI/RHIInterface/RHICommon.h"

class IRHITexture;

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

std::string RenderPassResourceTableName(RenderPassResourceTableId id);

#define RETURN_RESOURCE_TABLE_NAME(id) case RenderPassResourceTableId::id: return std::string(#id);
inline std::string RenderPassResourceTableName(RenderPassResourceTableId id)
{
    switch (id) {
        RETURN_RESOURCE_TABLE_NAME(Depth)
        RETURN_RESOURCE_TABLE_NAME(ScreenUVOffset)
        RETURN_RESOURCE_TABLE_NAME(RayTracingSceneOutput)
        RETURN_RESOURCE_TABLE_NAME(BasePass_Albedo)
        RETURN_RESOURCE_TABLE_NAME(BasePass_Normal)
        RETURN_RESOURCE_TABLE_NAME(LightingPass_Output)
        RETURN_RESOURCE_TABLE_NAME(RayTracingPass_ReSTIRSample_Output)
        RETURN_RESOURCE_TABLE_NAME(ComputePass_RayTracingOutputPostProcess_Output)
        RETURN_RESOURCE_TABLE_NAME(TestPass_Custom0)
        RETURN_RESOURCE_TABLE_NAME(TestPass_Custom1)
        RETURN_RESOURCE_TABLE_NAME(TestPass_Custom2)
        RETURN_RESOURCE_TABLE_NAME(TestPass_Custom3)
    }

    // Must add enum case to this function
    GLTF_CHECK(false);
    return "Unknown";
}

// Declare resource export/import info for specific render pass
struct RenderPassResourceTable
{
    std::vector<std::pair<RHITextureDesc, RenderPassResourceTableId>> m_export_texture_table_entry;
    std::map<RenderPassResourceTableId, RHITextureDescriptorDesc> m_export_descriptor_table_entry;
    
    std::vector<std::pair<RHITextureDesc, RenderPassResourceTableId>> m_import_texture_table_entry;
    std::map<RenderPassResourceTableId, RHITextureDescriptorDesc> m_import_descriptor_table_entry;
};

struct RenderPassResourceLocation
{
    std::map<RenderPassResourceTableId, std::vector<std::shared_ptr<IRHITexture>>> m_texture_allocations;
    std::map<RenderPassResourceTableId, std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>>> m_texture_descriptor_allocations;
};
