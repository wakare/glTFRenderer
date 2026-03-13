#pragma once

#include "RHICommon.h"

enum class RHIGraphicsAPIType
{
    RHI_GRAPHICS_API_DX12,
    RHI_GRAPHICS_API_Vulkan,
    RHI_GRAPHICS_API_NUM,
};

struct RHIVulkanOptionalFeatureRequirements
{
    bool require_ray_tracing_pipeline{false};
    bool require_ray_query{false};
};

class RHICORE_API RHIConfigSingleton
{
public:
    static RHIConfigSingleton& Instance()
    {
        static RHIConfigSingleton _instance;
        return _instance;
    }

    static bool InitGraphicsAPI();
    
    RHIGraphicsAPIType GetGraphicsAPIType() const {return m_graphics_api;}
    void SetGraphicsAPIType(RHIGraphicsAPIType type) {m_graphics_api = type; }
    bool IsAPIValidationEnabled() const { return m_enable_api_validation; }
    bool IsDX12GPUBasedValidationEnabled() const { return m_enable_dx12_gpu_validation; }
    bool IsDX12SynchronizedQueueValidationEnabled() const { return m_enable_dx12_sync_queue_validation; }
    void SetVulkanOptionalFeatureRequirements(const RHIVulkanOptionalFeatureRequirements& requirements) { m_vulkan_optional_feature_requirements = requirements; }
    const RHIVulkanOptionalFeatureRequirements& GetVulkanOptionalFeatureRequirements() const { return m_vulkan_optional_feature_requirements; }
    
private:
    RHIConfigSingleton();

    RHIGraphicsAPIType m_graphics_api;
    bool m_enable_api_validation{false};
    bool m_enable_dx12_gpu_validation{false};
    bool m_enable_dx12_sync_queue_validation{false};
    RHIVulkanOptionalFeatureRequirements m_vulkan_optional_feature_requirements{};
};
