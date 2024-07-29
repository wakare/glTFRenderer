#include "RHIConfigSingleton.h"

RHIConfigSingleton::RHIConfigSingleton()
    : m_graphics_api(RHIGraphicsAPIType::RHI_GRAPHICS_API_DX12)
{
}
