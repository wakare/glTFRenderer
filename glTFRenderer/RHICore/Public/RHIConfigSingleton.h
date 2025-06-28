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
    
private:
    RHIConfigSingleton();

    RHIGraphicsAPIType m_graphics_api;
};
