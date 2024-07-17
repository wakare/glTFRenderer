#include "VKDevice.h"

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <optional>
#include <set>

#include "VKFactory.h"
#include "RenderWindow/glTFWindow.h"

namespace VulkanEngineRequirements
{
    const std::vector validation_layers =
    {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector device_extensions =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkPhysicalDeviceVulkan13Features require_feature13
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = nullptr,
        .synchronization2 = true,
        .dynamicRendering = true,
    };

    VkPhysicalDeviceVulkan12Features require_feature12
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &require_feature13,
        .descriptorIndexing = true,
        .bufferDeviceAddress = true,
    };

    bool IsMatchRequireFeatures(const VkPhysicalDeviceVulkan12Features& device_features2, const VkPhysicalDeviceVulkan13Features& device_features3)
    {
        return device_features2.descriptorIndexing == require_feature12.descriptorIndexing
            && device_features2.bufferDeviceAddress == require_feature12.bufferDeviceAddress
            && device_features3.synchronization2 == require_feature13.synchronization2
            && device_features3.dynamicRendering == require_feature13.dynamicRendering;
    }
}


namespace VulkanEngineQueryStorage
{
    VkPhysicalDeviceVulkan13Features query_features13
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = nullptr,
    };
    
    VkPhysicalDeviceVulkan12Features query_feature12
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &query_features13,
    };

    VkPhysicalDeviceFeatures2 query_features2
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &query_feature12
    };
    
    QueueFamilyIndices queue_family_indices;
}


QueueFamilyIndices VKDevice::FindQueueFamily(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    unsigned queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_family_properties.data());

    QueueFamilyIndices result {};
    unsigned family_index = 0;
    for (const auto& queue_family_property : queue_family_properties)
    {
        if (queue_family_property.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            result.graphics_family = family_index; 
        }

        VkBool32 present_support = false;
        const VkResult success = vkGetPhysicalDeviceSurfaceSupportKHR(device, family_index, surface, &present_support);
        GLTF_CHECK(success == VK_SUCCESS);

        if (present_support)
        {
            result.present_family = family_index;
        }

        if (result.IsComplete())
        {
            break;
        }
        
        ++family_index;
    }

    return result;
}


SwapChainSupportDetails VKDevice::QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    unsigned format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);

    if (format_count)
    {
        details.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, details.formats.data());    
    }

    unsigned present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);

    if (present_mode_count)
    {
        details.present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, details.present_modes.data());    
    }
    
    return details;
}

// Check physical device
bool VKDevice::IsSuitableDevice(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    bool is_suitable = true;
    
    VkPhysicalDeviceProperties device_properties {};
    VkPhysicalDeviceFeatures device_features {};

    vkGetPhysicalDeviceProperties(device, &device_properties);
    vkGetPhysicalDeviceFeatures(device, &device_features);
    if (device_properties.apiVersion >= VK_MAKE_VERSION(1, 1, 0))
    {
        vkGetPhysicalDeviceFeatures2(device, &VulkanEngineQueryStorage::query_features2);
    }

    is_suitable &= (device_properties.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
        && device_features.geometryShader
        );

    // Check feature12 and feature13
    is_suitable &= VulkanEngineRequirements::IsMatchRequireFeatures(VulkanEngineQueryStorage::query_feature12, VulkanEngineQueryStorage::query_features13);
    is_suitable &= FindQueueFamily(device, surface).IsComplete();

    // Check device extensions
    unsigned extension_count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

    std::vector<VkExtensionProperties> extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, extensions.data());

    std::set<std::string> require_extensions (VulkanEngineRequirements::device_extensions.begin(), VulkanEngineRequirements::device_extensions.end());
    for (const auto& extension : extensions)
    {
        require_extensions.erase(extension.extensionName);
    }
    is_suitable &= require_extensions.empty();

    if (is_suitable)
    {
        SwapChainSupportDetails detail = QuerySwapChainSupport(device, surface);
        is_suitable &= !detail.formats.empty() && !detail.present_modes.empty();
    }
    
    return is_suitable;
}

VKDevice::VKDevice()
    : instance(VK_NULL_HANDLE)
    , surface(VK_NULL_HANDLE)
    , selected_physical_device(VK_NULL_HANDLE)
    , logical_device(VK_NULL_HANDLE)
    , graphics_queue_index(0)
    , present_queue_index(0)
{
    
}

VKDevice::~VKDevice()
{
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyDevice(logical_device, nullptr);
}

bool VKDevice::InitDevice(IRHIFactory& factory)
{
    instance = dynamic_cast<VKFactory&>(factory).GetInstance();
     
    // Create surface
    VkWin32SurfaceCreateInfoKHR create_surface_info{};
    create_surface_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    create_surface_info.hwnd = glTFWindow::Get().GetHWND();
    create_surface_info.hinstance = GetModuleHandle(NULL);

    VkResult result = vkCreateWin32SurfaceKHR(instance, &create_surface_info, nullptr, &surface);
    GLTF_CHECK(result == VK_SUCCESS);
    
    // Pick up physics device
    unsigned device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);

    if (!device_count)
    {
        GLTF_CHECK(false);
        return false;
    }

    std::vector<VkPhysicalDevice> physical_devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, physical_devices.data());

    selected_physical_device = VK_NULL_HANDLE;
    for (const auto& physical_device : physical_devices)
    {
        if (IsSuitableDevice(physical_device, surface))
        {
            selected_physical_device = physical_device;
            break;    
        }
    }

    if (selected_physical_device == VK_NULL_HANDLE)
    {
        GLTF_CHECK(false);
        return false;
    }

    // Create logic device
    const QueueFamilyIndices queue_family_indices = FindQueueFamily(selected_physical_device, surface);
    GLTF_CHECK(queue_family_indices.graphics_family.has_value());

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<unsigned> unique_queue_families = {queue_family_indices.graphics_family.value(), queue_family_indices.present_family.value()};
    const float queue_priority = 1.0f;

    for (const auto unique_queue_family : unique_queue_families)
    {
        VkDeviceQueueCreateInfo queue_create_info {};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = unique_queue_family;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_create_info);
    }

    // Non features to require now
    VkDeviceCreateInfo create_device_info{};
    create_device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_device_info.pQueueCreateInfos = queue_create_infos.data();
    create_device_info.queueCreateInfoCount = static_cast<unsigned>(queue_create_infos.size());
    create_device_info.ppEnabledLayerNames = VulkanEngineRequirements::validation_layers.data();
    create_device_info.enabledLayerCount = static_cast<unsigned>(VulkanEngineRequirements::validation_layers.size());
    create_device_info.ppEnabledExtensionNames = VulkanEngineRequirements::device_extensions.data();
    create_device_info.enabledExtensionCount = static_cast<unsigned>(VulkanEngineRequirements::device_extensions.size());
    
    create_device_info.pNext = &VulkanEngineQueryStorage::query_features2;
    
    result = vkCreateDevice(selected_physical_device, &create_device_info, nullptr, &logical_device); 
    GLTF_CHECK(result == VK_SUCCESS);

    graphics_queue_index = queue_family_indices.graphics_family.value();
    present_queue_index = queue_family_indices.present_family.value();

    // TODO: Ensure graphics and present queue are the same now
    GLTF_CHECK(graphics_queue_index == present_queue_index);
    
    return true;
}
