#pragma once

enum class RHIGraphicsAPIType
{
    RHI_GRAPHICS_API_DX12,
    RHI_GRAPHICS_API_NUM,
};

class RHIConfigSingleton
{
public:
    static RHIConfigSingleton& Instance()
    {
        static RHIConfigSingleton _instance;
        return _instance;
    }

    RHIGraphicsAPIType GetGraphicsAPIType() const {return m_graphicsAPI;}

private:
    RHIConfigSingleton();

    RHIGraphicsAPIType m_graphicsAPI;
};
