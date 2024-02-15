#include "glTFVulkanTest.h"

#include <optional>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <algorithm>
#include <set>
#include <GLFW/glfw3native.h>

#include "glTFWindow/glTFWindow.h"

const std::vector validation_layers =
{
    "VK_LAYER_KHRONOS_validation"
};

const std::vector device_extensions =
{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// Validation layer utils
bool CheckVulkanValidationLayerSupport()
{
    unsigned layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    for (const char* layer_name : validation_layers)
    {
        bool layer_found = false;
        for (const auto& layer_property : available_layers)
        {
            // Equal?
            if (strcmp(layer_name, layer_property.layerName) == 0)
            {
                layer_found = true;
                break;
            }
        }

        if (!layer_found)
        {
            return false;
        }
    }

    return true;
}

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
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

// Check queue family valid
struct QueueFamilyIndices
{
    std::optional<unsigned> graphics_family;
    std::optional<unsigned> present_family;

    bool IsComplete() const
    {
        return graphics_family.has_value() && present_family.has_value();
    }
};

QueueFamilyIndices FindQueueFamily(VkPhysicalDevice device, VkSurfaceKHR surface)
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

// Check physical device
bool IsSuitableDevice(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    bool is_suitable = true;
    
    VkPhysicalDeviceProperties device_properties;
    VkPhysicalDeviceFeatures device_features;

    vkGetPhysicalDeviceProperties(device, &device_properties);
    vkGetPhysicalDeviceFeatures(device, &device_features);

    is_suitable &= (device_properties.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
        device_features.geometryShader);

    is_suitable &= FindQueueFamily(device, surface).IsComplete();

    // Check device extensions
    unsigned extension_count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

    std::vector<VkExtensionProperties> extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, extensions.data());

    std::set<std::string> require_extensions (device_extensions.begin(), device_extensions.end());
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

VkSurfaceFormatKHR chooseSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& surface_formats)
{
    GLTF_CHECK(!surface_formats.empty());
    
    for (const auto& format : surface_formats)
    {
        if (format.format == VK_FORMAT_B8G8R8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return format;
        }
    }

    return surface_formats.front();
}

VkPresentModeKHR chooseSwapChainPresentMode(const std::vector<VkPresentModeKHR>& present_modes)
{
    GLTF_CHECK(!present_modes.empty());

    for (const auto& mode : present_modes)
    {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return mode;
        }
    }

    return present_modes.front();
}

VkExtent2D chooseSwapChainExtent(const VkSurfaceCapabilitiesKHR capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<unsigned>::max())
    {
        return capabilities.currentExtent;
    }

    unsigned width = glTFWindow::Get().GetWidth();
    unsigned height = glTFWindow::Get().GetHeight();

    VkExtent2D result = { width, height};
    result.width = std::clamp(result.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    result.height = std::clamp(result.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return result;
}

bool glTFVulkanTest::Init()
{
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    LOG_FORMAT_FLUSH("Vulkan Init: %d extensions found\n", extensionCount);

    // Check validation layer support
    const bool validation_layer_support = CheckVulkanValidationLayerSupport();
    GLTF_CHECK(validation_layer_support);
    
    // Create vulkan instance
    VkApplicationInfo app_info{};
    app_info.sType = VkStructureType::VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Hello Triangle (Vulkan)";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    unsigned glfw_extension_count = 0;
    const char** glfw_extension_names = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    for (unsigned i = 0; i < glfw_extension_count; ++i)
    {
        LOG_FORMAT_FLUSH("GLFW_REQUIRE_VK_EXTENSION: %s\n", glfw_extension_names[i]);
    }
    
    create_info.enabledExtensionCount = glfw_extension_count;
    create_info.ppEnabledExtensionNames = glfw_extension_names;
    
    create_info.enabledLayerCount = static_cast<unsigned>(validation_layers.size());
    create_info.ppEnabledLayerNames = validation_layers.data();

    VkResult result = vkCreateInstance(&create_info, nullptr, &instance);
    GLTF_CHECK(result == VK_SUCCESS);

    // Create surface
    VkWin32SurfaceCreateInfoKHR create_surface_info{};
    create_surface_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    create_surface_info.hwnd = glTFWindow::Get().GetHWND();
    create_surface_info.hinstance = GetModuleHandle(NULL);

    result = vkCreateWin32SurfaceKHR(instance, &create_surface_info, nullptr, &surface);
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

    select_physical_device = VK_NULL_HANDLE;
    for (const auto& physical_device : physical_devices)
    {
        if (IsSuitableDevice(physical_device, surface))
        {
            select_physical_device = physical_device;
            break;    
        }
    }

    if (select_physical_device == VK_NULL_HANDLE)
    {
        GLTF_CHECK(false);
        return false;
    }

    // Create logic device
    const QueueFamilyIndices queue_family_indices = FindQueueFamily(select_physical_device, surface);
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
    VkPhysicalDeviceFeatures device_features {};
    
    VkDeviceCreateInfo create_device_info{};
    create_device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_device_info.pQueueCreateInfos = queue_create_infos.data();
    create_device_info.queueCreateInfoCount = static_cast<unsigned>(queue_create_infos.size());
    create_device_info.pEnabledFeatures = &device_features;
    create_device_info.ppEnabledLayerNames = validation_layers.data();
    create_device_info.enabledLayerCount = static_cast<unsigned>(validation_layers.size());
    create_device_info.ppEnabledExtensionNames = device_extensions.data();
    create_device_info.enabledExtensionCount = static_cast<unsigned>(device_extensions.size());
    
    result = vkCreateDevice(select_physical_device, &create_device_info, nullptr, &logical_device); 
    GLTF_CHECK(result == VK_SUCCESS);

    vkGetDeviceQueue(logical_device, queue_family_indices.graphics_family.value(), 0, &graphics_queue);
    vkGetDeviceQueue(logical_device, queue_family_indices.present_family.value(), 0, &present_queue);

    // Create SwapChain
    SwapChainSupportDetails detail = QuerySwapChainSupport(select_physical_device, surface);

    VkSurfaceFormatKHR format = chooseSwapChainSurfaceFormat(detail.formats);
    VkPresentModeKHR mode = chooseSwapChainPresentMode(detail.present_modes);
    VkExtent2D extent = chooseSwapChainExtent(detail.capabilities);

    // Add one image for back buffer, otherwise wait for last frame rendering
    unsigned image_count = detail.capabilities.minImageCount + 1;
    if (detail.capabilities.maxImageCount > 0 && image_count > detail.capabilities.maxImageCount)
    {
        image_count = detail.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_swap_chain_info {};
    create_swap_chain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_swap_chain_info.surface = surface;
    create_swap_chain_info.minImageCount = image_count;
    create_swap_chain_info.imageFormat = format.format;
    create_swap_chain_info.imageColorSpace = format.colorSpace;
    create_swap_chain_info.imageExtent = extent;
    create_swap_chain_info.imageArrayLayers = 1;
    create_swap_chain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    if (queue_family_indices.graphics_family.value() != queue_family_indices.present_family.value())
    {
        unsigned family_indices[] = {queue_family_indices.graphics_family.value(), queue_family_indices.present_family.value()}; 
        create_swap_chain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_swap_chain_info.queueFamilyIndexCount = 2;
        create_swap_chain_info.pQueueFamilyIndices = family_indices;
    }
    else
    {
        create_swap_chain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_swap_chain_info.queueFamilyIndexCount = 0;
        create_swap_chain_info.pQueueFamilyIndices = nullptr;
    }

    create_swap_chain_info.preTransform = detail.capabilities.currentTransform;
    create_swap_chain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_swap_chain_info.presentMode = mode;
    create_swap_chain_info.clipped = VK_TRUE;
    create_swap_chain_info.oldSwapchain = VK_NULL_HANDLE;

    result = vkCreateSwapchainKHR(logical_device, &create_swap_chain_info, nullptr, &swap_chain);
    GLTF_CHECK(result == VK_SUCCESS);

    unsigned swap_chain_image_count = 0;
    vkGetSwapchainImagesKHR(logical_device, swap_chain, &swap_chain_image_count, nullptr);

    if (swap_chain_image_count)
    {
        images.resize(swap_chain_image_count);
        vkGetSwapchainImagesKHR(logical_device, swap_chain, &swap_chain_image_count, images.data());
    }

    swap_chain_format = format.format;
    swap_chain_extent = extent; 
    
    image_views.resize(images.size());
    for (unsigned i = 0; i < image_count; ++i)
    {
        VkImageViewCreateInfo create_image_view_info{};
        create_image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_image_view_info.image = images[i];
        create_image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_image_view_info.format = swap_chain_format;
        create_image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_image_view_info.subresourceRange.baseMipLevel = 0;
        create_image_view_info.subresourceRange.levelCount = 1;
        create_image_view_info.subresourceRange.baseArrayLayer = 0;
        create_image_view_info.subresourceRange.layerCount = 1;

        result = vkCreateImageView(logical_device, &create_image_view_info, nullptr, &image_views[i]);
        GLTF_CHECK(result == VK_SUCCESS);
    }
    
    return true;
}

bool glTFVulkanTest::Update()
{
    return true;
}

bool glTFVulkanTest::UnInit()
{
    for (const auto& image_view : image_views)
    {
        vkDestroyImageView(logical_device, image_view, nullptr);    
    }
    
    vkDestroySwapchainKHR(logical_device, swap_chain, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyDevice(logical_device, nullptr);
    vkDestroyInstance(instance, nullptr);    
    
    return true;
}
