#include "glTFVulkanTest.h"
#include <vulkan/vulkan.h>

bool glTFVulkanTest::Init()
{
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    LOG_FORMAT_FLUSH("Vulkan Init: %d extensions found", extensionCount);
    
    return true;
}

bool glTFVulkanTest::Update()
{
    return true;
}
