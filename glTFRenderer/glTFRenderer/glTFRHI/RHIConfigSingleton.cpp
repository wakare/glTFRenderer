#include "RHIConfigSingleton.h"

#include "RHIUtils.h"

bool RHIConfigSingleton::InitGraphicsAPI()
{
    return RHIUtils::Instance().InitGraphicsAPI();
}

RHIConfigSingleton::RHIConfigSingleton()
    : m_graphics_api(RHIGraphicsAPIType::RHI_GRAPHICS_API_DX12)
{
}
