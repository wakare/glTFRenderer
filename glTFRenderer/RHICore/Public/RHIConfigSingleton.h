#pragma once

#include "RHICommon.h"

enum class RHIGraphicsAPIType
{
    RHI_GRAPHICS_API_DX12,
    RHI_GRAPHICS_API_Vulkan,
    RHI_GRAPHICS_API_NUM,
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
    
private:
    RHIConfigSingleton();

    RHIGraphicsAPIType m_graphics_api;
    bool m_enable_api_validation{false};
    bool m_enable_dx12_gpu_validation{false};
    bool m_enable_dx12_sync_queue_validation{false};
};
