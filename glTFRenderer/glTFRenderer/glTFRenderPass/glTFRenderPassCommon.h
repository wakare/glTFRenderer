#pragma once

#include <string>
#include <glm/glm/gtx/compatibility.hpp>

#include "RendererCommon.h"
#include "RHICommon.h"

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

enum RenderPassResourceTableId
{
    // Common
    Depth = 0,
    ScreenUVOffset = 1,
    RayTracingSceneOutput =2,
    
    // Pass specific
    BasePass_Albedo = 3,
    BasePass_Normal = 4,
    LightingPass_Output = 6,
    RayTracingPass_ReSTIRSample_Output = 7,
    ComputePass_RayTracingOutputPostProcess_Output = 8,
    ShadowPass_Output = 9,
    
    // Custom for test pass
    TestPass_Custom0 = 10,
    TestPass_Custom1 = TestPass_Custom0 + 1,
    TestPass_Custom2 = TestPass_Custom0 + 2,
    TestPass_Custom3 = TestPass_Custom0 + 3,

    // VT Feedback [max 100 feedback texture] 
    VT_FeedBack_Base_Index = 100,
    VT_FeedBack_MAX_Index = 200,
};

inline RenderPassResourceTableId GetVTFeedBackId(unsigned vt_id)
{
    return static_cast<RenderPassResourceTableId>(VT_FeedBack_Base_Index + vt_id);
}

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
        RETURN_RESOURCE_TABLE_NAME(ShadowPass_Output)
        RETURN_RESOURCE_TABLE_NAME(TestPass_Custom0)
        RETURN_RESOURCE_TABLE_NAME(TestPass_Custom1)
        RETURN_RESOURCE_TABLE_NAME(TestPass_Custom2)
        RETURN_RESOURCE_TABLE_NAME(TestPass_Custom3)
    }

    if (id >= VT_FeedBack_Base_Index && VT_FeedBack_MAX_Index > id)
    {
        return "VT_FeedBack_Base_Index_" + std::to_string(id - VT_FeedBack_Base_Index);
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

    std::map<unsigned, RHITextureDescriptorDesc> m_local_texture_descriptor_table_entry;
    std::map<unsigned, RHITextureDesc> m_local_texture_table_entry;
};

struct RenderPassResourceLocation
{
    std::map<RenderPassResourceTableId, std::vector<std::shared_ptr<IRHITexture>>> m_texture_allocations;
    std::map<RenderPassResourceTableId, std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>>> m_texture_descriptor_allocations;

    std::map<unsigned, std::vector<std::shared_ptr<IRHITexture>>> m_local_texture_allocations;
    std::map<unsigned, std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>>> m_local_texture_descriptor_allocations;
};
