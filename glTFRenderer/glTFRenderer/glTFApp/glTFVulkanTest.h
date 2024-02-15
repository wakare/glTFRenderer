#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "glTFUtils/glTFUtils.h"

class glTFVulkanTest
{
public:
    glTFVulkanTest() = default;
    ~glTFVulkanTest() = default;
    DECLARE_NON_COPYABLE(glTFVulkanTest)
    
    bool Init();
    bool Update();
    bool UnInit();

protected:
    VkInstance instance;
    VkPhysicalDevice select_physical_device;
    VkDevice logical_device;
    VkQueue graphics_queue;
    VkSurfaceKHR surface;
    VkQueue present_queue;
    VkSwapchainKHR swap_chain;
    std::vector<VkImage> images;
    VkFormat swap_chain_format;
    VkExtent2D swap_chain_extent;
    
    std::vector<VkImageView> image_views;
};
