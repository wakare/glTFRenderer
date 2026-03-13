#define VK_USE_PLATFORM_WIN32_KHR
#include "VKDevice.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <optional>
#include <set>

#include "RHIConfigSingleton.h"
#include "VKFactory.h"
#include "RenderWindow/glTFWindow.h"

namespace VulkanEngineRequirements
{
    const std::vector validation_layers =
    {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector required_device_extensions =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    const std::vector optional_present_extensions =
    {
        VK_KHR_PRESENT_WAIT_EXTENSION_NAME,
        VK_KHR_PRESENT_ID_EXTENSION_NAME
    };

    bool RequiresPromotedVulkan12Extensions(uint32_t api_version)
    {
        const uint32_t major = VK_API_VERSION_MAJOR(api_version);
        const uint32_t minor = VK_API_VERSION_MINOR(api_version);
        return major < 1 || (major == 1 && minor < 2);
    }

    std::vector<const char*> BuildRequestedRayTracingExtensions(
        uint32_t api_version,
        bool require_ray_tracing_pipeline,
        bool require_ray_query)
    {
        std::vector<const char*> extensions;
        const bool require_acceleration_structure = require_ray_tracing_pipeline || require_ray_query;
        if (!require_acceleration_structure)
        {
            return extensions;
        }

        extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);

        const bool require_promoted_vulkan12_extensions = RequiresPromotedVulkan12Extensions(api_version);
        if (require_promoted_vulkan12_extensions)
        {
            extensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
            extensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
        }

        if (require_ray_tracing_pipeline)
        {
            extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
            if (require_promoted_vulkan12_extensions)
            {
                extensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
            }
        }

        if (require_ray_query)
        {
            extensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
        }

        return extensions;
    }

    VkPhysicalDeviceVulkan13Features require_feature13
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = true,
        .dynamicRendering = true,
    };

    VkPhysicalDeviceVulkan12Features require_feature12
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .descriptorIndexing = true,
        .bufferDeviceAddress = true,
    };

    bool IsMatchRequireFeatures(
        const VkPhysicalDeviceVulkan12Features& device_features2,
        const VkPhysicalDeviceVulkan13Features& device_features3)
    {
        return device_features2.descriptorIndexing == require_feature12.descriptorIndexing
            && device_features2.bufferDeviceAddress == require_feature12.bufferDeviceAddress
            && device_features3.synchronization2 == require_feature13.synchronization2
            && device_features3.dynamicRendering == require_feature13.dynamicRendering;
    }

    struct DeviceFeatureQueryChain
    {
        VkPhysicalDeviceVulkan13Features feature13
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .pNext = nullptr,
        };

        VkPhysicalDeviceVulkan12Features feature12
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = &feature13,
        };

        VkPhysicalDeviceFeatures2 features2
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &feature12,
        };
    };

    struct DeviceFeatureEnableChain
    {
        VkPhysicalDeviceVulkan13Features feature13
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .pNext = nullptr,
            .synchronization2 = VulkanEngineRequirements::require_feature13.synchronization2,
            .dynamicRendering = VulkanEngineRequirements::require_feature13.dynamicRendering,
        };

        VkPhysicalDeviceVulkan12Features feature12
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = &feature13,
            .descriptorIndexing = VulkanEngineRequirements::require_feature12.descriptorIndexing,
            .bufferDeviceAddress = VulkanEngineRequirements::require_feature12.bufferDeviceAddress,
        };

        VkPhysicalDeviceFeatures2 features2
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &feature12,
        };
    };

    bool HasRequiredExtensions(VkPhysicalDevice device, const std::vector<const char*>& required_extensions)
    {
        unsigned extension_count = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

        std::vector<VkExtensionProperties> extensions(extension_count);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, extensions.data());

        std::set<std::string> missing_extensions(required_extensions.begin(), required_extensions.end());
        for (const auto& extension : extensions)
        {
            missing_extensions.erase(extension.extensionName);
        }

        return missing_extensions.empty();
    }
}


bool VKDevice::Release(IRHIMemoryManager& memory_manager)
{
    if (!need_release)
    {
        return true;
    }

    need_release = false;
    if (logical_device != VK_NULL_HANDLE)
    {
        const VkResult wait_result = vkDeviceWaitIdle(logical_device);
        if (wait_result != VK_SUCCESS)
        {
            LOG_FORMAT_FLUSH("[VKDevice] vkDeviceWaitIdle during release returned %d.\n", static_cast<int>(wait_result));
        }
        vkDestroyDevice(logical_device, nullptr);
        logical_device = VK_NULL_HANDLE;
    }
    if (surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
    }

    selected_physical_device = VK_NULL_HANDLE;
    instance = VK_NULL_HANDLE;
    graphics_queue_index = 0;
    present_queue_index = 0;
    m_ray_tracing_supported = false;
    m_ray_query_supported = false;
    m_present_wait_supported = false;
    m_rt_pipeline_properties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
    
    return true;
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
    VulkanEngineRequirements::DeviceFeatureQueryChain feature_query{};
    if (device_properties.apiVersion >= VK_MAKE_VERSION(1, 1, 0))
    {
        vkGetPhysicalDeviceFeatures2(device, &feature_query.features2);
    }

    is_suitable &= (device_properties.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
        && device_features.geometryShader
        );

    // Check feature12 and feature13
    is_suitable &= VulkanEngineRequirements::IsMatchRequireFeatures(
        feature_query.feature12,
        feature_query.feature13
        );
    is_suitable &= FindQueueFamily(device, surface).IsComplete();

    // Check device extensions
    is_suitable &= VulkanEngineRequirements::HasRequiredExtensions(
        device,
        VulkanEngineRequirements::required_device_extensions);

    if (is_suitable)
    {
        SwapChainSupportDetails detail = QuerySwapChainSupport(device, surface);
        is_suitable &= !detail.formats.empty() && !detail.present_modes.empty();
    }
    
    return is_suitable;
}

bool VKDevice::CheckPresentWaitSupport(VkPhysicalDevice device)
{
    if (!VulkanEngineRequirements::HasRequiredExtensions(device, VulkanEngineRequirements::optional_present_extensions))
    {
        return false;
    }

    VkPhysicalDevicePresentIdFeaturesKHR present_id_features
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR,
        .pNext = nullptr,
    };

    VkPhysicalDevicePresentWaitFeaturesKHR present_wait_features
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR,
        .pNext = &present_id_features,
    };

    VkPhysicalDeviceFeatures2 features2
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &present_wait_features,
    };

    vkGetPhysicalDeviceFeatures2(device, &features2);
    return present_wait_features.presentWait == VK_TRUE && present_id_features.presentId == VK_TRUE;
}

bool VKDevice::CheckRayTracingSupport(
    VkPhysicalDevice device,
    uint32_t api_version,
    bool require_ray_tracing_pipeline,
    bool require_ray_query)
{
    const bool require_acceleration_structure = require_ray_tracing_pipeline || require_ray_query;
    if (!require_acceleration_structure)
    {
        return true;
    }

    const auto required_extensions = VulkanEngineRequirements::BuildRequestedRayTracingExtensions(
        api_version,
        require_ray_tracing_pipeline,
        require_ray_query);
    if (!VulkanEngineRequirements::HasRequiredExtensions(device, required_extensions))
    {
        return false;
    }

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt_pipeline_features
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .pNext = nullptr,
    };

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accel_features
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .pNext = nullptr,
    };

    VkPhysicalDeviceRayQueryFeaturesKHR ray_query_features
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
        .pNext = nullptr,
    };

    VkPhysicalDeviceFeatures2 features2
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = nullptr,
    };

    if (require_ray_tracing_pipeline)
    {
        features2.pNext = &rt_pipeline_features;
        rt_pipeline_features.pNext = &accel_features;
        accel_features.pNext = require_ray_query ? &ray_query_features : nullptr;
    }
    else
    {
        features2.pNext = &accel_features;
        accel_features.pNext = require_ray_query ? &ray_query_features : nullptr;
    }

    vkGetPhysicalDeviceFeatures2(device, &features2);
    if ((require_ray_tracing_pipeline && rt_pipeline_features.rayTracingPipeline != VK_TRUE) ||
        (require_acceleration_structure && accel_features.accelerationStructure != VK_TRUE) ||
        (require_ray_query && ray_query_features.rayQuery != VK_TRUE))
    {
        return false;
    }

    if (!require_ray_tracing_pipeline)
    {
        return true;
    }

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_pipeline_props
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
        .pNext = nullptr,
    };

    VkPhysicalDeviceProperties2 props2
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rt_pipeline_props,
    };

    vkGetPhysicalDeviceProperties2(device, &props2);
    m_rt_pipeline_properties = rt_pipeline_props;

    return true;
}

bool VKDevice::InitDevice(IRHIFactory& factory)
{
    instance = dynamic_cast<VKFactory&>(factory).GetInstance();
    surface = VK_NULL_HANDLE;
    logical_device = VK_NULL_HANDLE;
    selected_physical_device = VK_NULL_HANDLE;
    graphics_queue_index = 0;
    present_queue_index = 0;
    m_ray_tracing_supported = false;
    m_ray_query_supported = false;
    m_present_wait_supported = false;
    m_rt_pipeline_properties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
     
    // Create surface
    VkWin32SurfaceCreateInfoKHR create_surface_info{};
    create_surface_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    const HWND hwnd = glTFWindow::Get().GetHWND();
    GLTF_CHECK(hwnd != nullptr);
    create_surface_info.hwnd = hwnd;
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

    VkPhysicalDeviceProperties selected_device_properties{};
    vkGetPhysicalDeviceProperties(selected_physical_device, &selected_device_properties);

    const bool present_wait_capable = CheckPresentWaitSupport(selected_physical_device);
    const auto& optional_feature_requirements = RHIConfigSingleton::Instance().GetVulkanOptionalFeatureRequirements();
    const bool require_ray_tracing_pipeline = optional_feature_requirements.require_ray_tracing_pipeline;
    const bool require_ray_query = optional_feature_requirements.require_ray_query;
    const bool require_any_ray_tracing_capability = require_ray_tracing_pipeline || require_ray_query;
    if (require_any_ray_tracing_capability &&
        !CheckRayTracingSupport(
            selected_physical_device,
            selected_device_properties.apiVersion,
            require_ray_tracing_pipeline,
            require_ray_query))
    {
        LOG_FORMAT_FLUSH(
            "[VKDevice] Required Vulkan ray tracing capability unsupported. pipeline=%d ray_query=%d api=%u.%u.%u\n",
            static_cast<int>(require_ray_tracing_pipeline),
            static_cast<int>(require_ray_query),
            VK_API_VERSION_MAJOR(selected_device_properties.apiVersion),
            VK_API_VERSION_MINOR(selected_device_properties.apiVersion),
            VK_API_VERSION_PATCH(selected_device_properties.apiVersion));
        return false;
    }
    m_ray_tracing_supported = require_ray_tracing_pipeline;
    m_ray_query_supported = require_ray_query;

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

    unsigned enabled_extension_count = 0;
    const bool enable_api_validation = RHIConfigSingleton::Instance().IsAPIValidationEnabled();
    auto try_create_device = [&](bool enable_present_wait) -> VkResult
    {
        std::vector<const char*> enabled_extensions = VulkanEngineRequirements::required_device_extensions;
        if (enable_present_wait)
        {
            enabled_extensions.insert(enabled_extensions.end(),
                VulkanEngineRequirements::optional_present_extensions.begin(),
                VulkanEngineRequirements::optional_present_extensions.end());
        }
        const auto ray_tracing_extensions = VulkanEngineRequirements::BuildRequestedRayTracingExtensions(
            selected_device_properties.apiVersion,
            m_ray_tracing_supported,
            m_ray_query_supported);
        if (!ray_tracing_extensions.empty())
        {
            enabled_extensions.insert(enabled_extensions.end(),
                ray_tracing_extensions.begin(),
                ray_tracing_extensions.end());
        }

        VkDeviceCreateInfo create_device_info{};
        create_device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_device_info.pQueueCreateInfos = queue_create_infos.data();
        create_device_info.queueCreateInfoCount = static_cast<unsigned>(queue_create_infos.size());
        create_device_info.ppEnabledLayerNames = enable_api_validation ? VulkanEngineRequirements::validation_layers.data() : nullptr;
        create_device_info.enabledLayerCount = enable_api_validation ? static_cast<unsigned>(VulkanEngineRequirements::validation_layers.size()) : 0;
        create_device_info.ppEnabledExtensionNames = enabled_extensions.data();
        create_device_info.enabledExtensionCount = static_cast<unsigned>(enabled_extensions.size());

        VulkanEngineRequirements::DeviceFeatureEnableChain required_features{};
        required_features.feature13.pNext = nullptr;
        VkBaseOutStructure* feature_chain_tail = reinterpret_cast<VkBaseOutStructure*>(&required_features.feature13);
        auto append_feature = [&feature_chain_tail](auto& feature)
        {
            feature_chain_tail->pNext = reinterpret_cast<VkBaseOutStructure*>(&feature);
            feature_chain_tail = reinterpret_cast<VkBaseOutStructure*>(&feature);
            feature_chain_tail->pNext = nullptr;
        };

        VkPhysicalDevicePresentIdFeaturesKHR present_id_features
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR,
            .pNext = nullptr,
            .presentId = enable_present_wait,
        };

        VkPhysicalDevicePresentWaitFeaturesKHR present_wait_features
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR,
            .pNext = nullptr,
            .presentWait = enable_present_wait,
        };

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt_pipeline_features
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
            .pNext = nullptr,
            .rayTracingPipeline = m_ray_tracing_supported ? VK_TRUE : VK_FALSE,
        };

        VkPhysicalDeviceAccelerationStructureFeaturesKHR accel_features
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
            .pNext = nullptr,
            .accelerationStructure = (m_ray_tracing_supported || m_ray_query_supported) ? VK_TRUE : VK_FALSE,
        };

        VkPhysicalDeviceRayQueryFeaturesKHR ray_query_features
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
            .pNext = nullptr,
            .rayQuery = m_ray_query_supported ? VK_TRUE : VK_FALSE,
        };

        if (enable_present_wait)
        {
            append_feature(present_wait_features);
            append_feature(present_id_features);
        }

        if (m_ray_tracing_supported)
        {
            append_feature(rt_pipeline_features);
        }
        if (m_ray_tracing_supported || m_ray_query_supported)
        {
            append_feature(accel_features);
        }
        if (m_ray_query_supported)
        {
            append_feature(ray_query_features);
        }

        create_device_info.pNext = &required_features.features2;
        enabled_extension_count = static_cast<unsigned>(enabled_extensions.size());
        return vkCreateDevice(selected_physical_device, &create_device_info, nullptr, &logical_device);
    };

    m_present_wait_supported = present_wait_capable;
    result = try_create_device(m_present_wait_supported);
    if (result != VK_SUCCESS && m_present_wait_supported)
    {
        LOG_FORMAT_FLUSH("[VKDevice] vkCreateDevice failed with present_wait/present_id enabled. Retrying without them.\n");
        m_present_wait_supported = false;
        result = try_create_device(false);
    }

    if (result != VK_SUCCESS)
    {
        LOG_FORMAT_FLUSH("[VKDevice] vkCreateDevice failed. result=%d hwnd=%p surface=%p rt=%d ray_query=%d present_wait=%d ext_count=%u\n",
            static_cast<int>(result),
            hwnd,
            surface,
            static_cast<int>(m_ray_tracing_supported),
            static_cast<int>(m_ray_query_supported),
            static_cast<int>(m_present_wait_supported),
            enabled_extension_count);
    }
    GLTF_CHECK(result == VK_SUCCESS);

    VolkUtils::LoadDevice(logical_device);
    
    graphics_queue_index = queue_family_indices.graphics_family.value();
    present_queue_index = queue_family_indices.present_family.value();

    // TODO: Ensure graphics and present queue are the same now
    GLTF_CHECK(graphics_queue_index == present_queue_index);
    
    need_release = true;
    
    return true;
}
