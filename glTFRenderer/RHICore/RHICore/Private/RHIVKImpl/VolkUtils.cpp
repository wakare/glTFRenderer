#define VK_NO_PROTOTYPES
#define VOLK_IMPLEMENTATION
#define VK_USE_PLATFORM_WIN32_KHR
#include "VolkUtils.h"

bool VolkUtils::InitVolk()
{
    volkInitialize();
    return true;
}

bool VolkUtils::LoadDevice(VkDevice device)
{
    volkLoadDevice(device);
    return true;
}

bool VolkUtils::LoadInstance(VkInstance instance)
{
    volkLoadInstance(instance);
    return true;
}
