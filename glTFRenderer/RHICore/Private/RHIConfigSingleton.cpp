#include "RHIConfigSingleton.h"

#include "RHIUtils.h"

bool RHIConfigSingleton::InitGraphicsAPI()
{
    RHIUtilInstanceManager::ResetInstance();
    return RHIUtilInstanceManager::Instance().InitGraphicsAPI();
}

RHIConfigSingleton::RHIConfigSingleton()
    : m_graphics_api(RHIGraphicsAPIType::RHI_GRAPHICS_API_DX12)
{
}
