#include "RHIConfigSingleton.h"

RHIConfigSingleton::RHIConfigSingleton()
    : m_graphicsAPI(RHIGraphicsAPIType::RHI_GRAPHICS_API_DX12)
{
}
