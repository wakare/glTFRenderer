#include "RHIConfigSingleton.h"

#include <cctype>
#include <cstdlib>
#include <string>

#include "RHIUtils.h"

namespace
{
bool ReadEnvFlag(const char* name, bool default_value)
{
    char* value = nullptr;
    size_t value_length = 0;
    if (_dupenv_s(&value, &value_length, name) != 0 || !value || value[0] == '\0')
    {
        std::free(value);
        return default_value;
    }

    std::string normalized(value);
    std::free(value);
    for (char& ch : normalized)
    {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }

    if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on")
    {
        return true;
    }
    if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off")
    {
        return false;
    }

    return default_value;
}
}

bool RHIConfigSingleton::InitGraphicsAPI()
{
    RHIUtilInstanceManager::ResetInstance();
    return RHIUtilInstanceManager::Instance().InitGraphicsAPI();
}

RHIConfigSingleton::RHIConfigSingleton()
    : m_graphics_api(RHIGraphicsAPIType::RHI_GRAPHICS_API_DX12)
{
    m_enable_api_validation = ReadEnvFlag("GLTF_RHI_VALIDATION", false);
    m_enable_dx12_gpu_validation = ReadEnvFlag("GLTF_DX12_GPU_VALIDATION", false);
    m_enable_dx12_sync_queue_validation = ReadEnvFlag("GLTF_DX12_SYNC_QUEUE_VALIDATION", false);
    if (m_enable_dx12_gpu_validation || m_enable_dx12_sync_queue_validation)
    {
        m_enable_api_validation = true;
    }
}
