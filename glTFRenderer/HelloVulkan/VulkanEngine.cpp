#define NOMINMAX

#include "VulkanEngine.h"

#include <optional>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <algorithm>
#include <set>
#include <string>

#define VMA_IMPLEMENTATION
#include <array>

#include "vma/vk_mem_alloc.h"

#include "ShaderUtil/glTFShaderUtils.h"
#include "RenderWindow/glTFWindow.h"

#define VK_CHECK(x) {\
    VkResult result = (x);\
    assert(result == VK_SUCCESS);\
}

const int MAX_FRAMES_IN_FLIGHT = 2;



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

void DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type)
{
    VkDescriptorSetLayoutBinding newbind {};
    newbind.binding = binding;
    newbind.descriptorCount = 1;
    newbind.descriptorType = type;

    bindings.push_back(newbind);
}

void DescriptorLayoutBuilder::clear()
{
    bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext, VkDescriptorSetLayoutCreateFlags flags)
{
    for (auto& b : bindings) {
        b.stageFlags |= shaderStages;
    }

    VkDescriptorSetLayoutCreateInfo info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    info.pNext = pNext;

    info.pBindings = bindings.data();
    info.bindingCount = (uint32_t)bindings.size();
    info.flags = flags;

    VkDescriptorSetLayout set;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

    return set;
}
void DescriptorAllocator::init_pool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios)
{
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (PoolSizeRatio ratio : poolRatios) {
        poolSizes.push_back(VkDescriptorPoolSize{
            .type = ratio.type,
            .descriptorCount = uint32_t(ratio.ratio * maxSets)
        });
    }

    VkDescriptorPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_info.flags = 0;
    pool_info.maxSets = maxSets;
    pool_info.poolSizeCount = (uint32_t)poolSizes.size();
    pool_info.pPoolSizes = poolSizes.data();

    vkCreateDescriptorPool(device, &pool_info, nullptr, &pool);
}

void DescriptorAllocator::clear_descriptors(VkDevice device)
{
    vkResetDescriptorPool(device, pool, 0);
}

void DescriptorAllocator::destroy_pool(VkDevice device)
{
    vkDestroyDescriptorPool(device,pool,nullptr);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo allocInfo = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));

    return ds;
}

VkImageCreateInfo GetImageCreateInfo(VkFormat format, VkImageUsageFlags usage_flags, VkExtent3D extent)
{
    VkImageCreateInfo create_info {};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    create_info.pNext = nullptr;
    create_info.imageType = VK_IMAGE_TYPE_2D;
    create_info.format = format;
    create_info.extent = extent;
    create_info.mipLevels = 1;
    create_info.arrayLayers = 1;
    create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    create_info.usage = usage_flags;

    return create_info;
}

VkImageViewCreateInfo GetImageViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspect_flags)
{
    VkImageViewCreateInfo image_view_create_info{};
    image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_info.pNext = nullptr;
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.image = image;
    image_view_create_info.format = format;
    image_view_create_info.subresourceRange.baseMipLevel = 0;
    image_view_create_info.subresourceRange.levelCount = 1;
    image_view_create_info.subresourceRange.baseArrayLayer = 0;
    image_view_create_info.subresourceRange.layerCount = 1;
    image_view_create_info.subresourceRange.aspectMask = aspect_flags;

    return image_view_create_info;
}

void CopyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D src_size, VkExtent2D dst_size)
{
    VkImageBlit2 blit_region {.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr};
    blit_region.srcOffsets[1] = VkOffset3D{(int32_t)src_size.width, (int32_t)src_size.height, 1};
    blit_region.dstOffsets[1] = VkOffset3D{(int32_t)dst_size.width, (int32_t)dst_size.height, 1};
    blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit_region.srcSubresource.baseArrayLayer = 0;
    blit_region.srcSubresource.layerCount = 1;
    blit_region.srcSubresource.mipLevel = 0;

    blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit_region.dstSubresource.baseArrayLayer = 0;
    blit_region.dstSubresource.layerCount = 1;
    blit_region.dstSubresource.mipLevel = 0;

    VkBlitImageInfo2 blit_info {.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr};
    blit_info.dstImage = destination;
    blit_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    blit_info.srcImage = source;
    blit_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blit_info.filter = VK_FILTER_LINEAR;
    blit_info.regionCount = 1;
    blit_info.pRegions = &blit_region;

    vkCmdBlitImage2(cmd, &blit_info);
}

// Validation layer utils
bool CheckVulkanValidationLayerSupport()
{
    unsigned layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    for (const char* layer_name : VulkanEngineRequirements::validation_layers)
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

VkShaderModule CreateShaderModule(VkDevice device, const std::vector<unsigned char>& shader_binaries)
{
    VkShaderModuleCreateInfo create_shader_module_info{};
    create_shader_module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_shader_module_info.codeSize = shader_binaries.size();
    create_shader_module_info.pCode = (unsigned*)shader_binaries.data();

    VkShaderModule shader_module;
    VkResult result = vkCreateShaderModule(device, &create_shader_module_info, nullptr, &shader_module);
    GLTF_CHECK(result == VK_SUCCESS);

    return shader_module;
}

VkImageSubresourceRange GetVkSubresourceRange(VkImageAspectFlags aspect_mask)
{
    VkImageSubresourceRange sub_image{};
    sub_image.aspectMask = aspect_mask;
    sub_image.baseMipLevel = 0;
    sub_image.levelCount = VK_REMAINING_MIP_LEVELS;
    sub_image.baseArrayLayer = 0;
    sub_image.layerCount = VK_REMAINING_ARRAY_LAYERS;

    return sub_image;
}

void VulkanEngine::TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout old_layout,
    VkImageLayout new_layout)
{
    VkImageMemoryBarrier2 image_barrier {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    image_barrier.pNext = nullptr;
    image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    image_barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    image_barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

    image_barrier.oldLayout = old_layout;
    image_barrier.newLayout = new_layout;

    VkImageAspectFlags aspect_flags = (new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ?
        VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    image_barrier.subresourceRange = GetVkSubresourceRange(aspect_flags);
    image_barrier.image = image;
    
    VkDependencyInfo dep_info {};
    dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep_info.pNext = nullptr;
    dep_info.imageMemoryBarrierCount = 1;
    dep_info.pImageMemoryBarriers = &image_barrier;
    vkCmdPipelineBarrier2(cmd, &dep_info);
}

AllocatedBuffer VulkanEngine::AllocateBuffer(size_t allocate_size, VkBufferUsageFlags usage_flags,
    VmaMemoryUsage memory_usage)
{
    // allocate buffer
    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocate_size;

    bufferInfo.usage = usage_flags;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = memory_usage;
    vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    AllocatedBuffer newBuffer;

    // allocate the buffer
    VK_CHECK(vmaCreateBuffer(vma_allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation,
        &newBuffer.info));

    return newBuffer;
}

void VulkanEngine::DestroyBuffer(const AllocatedBuffer& buffer)
{
    vmaDestroyBuffer(vma_allocator, buffer.buffer, buffer.allocation);
}

GPUMeshBuffers VulkanEngine::UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    GPUMeshBuffers newSurface;

    //create vertex buffer
    newSurface.vertexBuffer = AllocateBuffer(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    //find the adress of the vertex buffer
    VkBufferDeviceAddressInfo deviceAdressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,.buffer = newSurface.vertexBuffer.buffer };
    newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(logical_device, &deviceAdressInfo);

    //create index buffer
    newSurface.indexBuffer = AllocateBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    AllocatedBuffer staging = AllocateBuffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = staging.allocation->GetMappedData();

    // copy vertex buffer
    memcpy(data, vertices.data(), vertexBufferSize);
    // copy index buffer
    memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

    ImmediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy{ 0 };
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = vertexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

        VkBufferCopy indexCopy{ 0 };
        indexCopy.dstOffset = 0;
        indexCopy.srcOffset = vertexBufferSize;
        indexCopy.size = indexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
    });

    DestroyBuffer(staging);

    return newSurface;
}

bool VulkanEngine::Init()
{
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    LOG_FORMAT_FLUSH("Vulkan Init: %d extensions found\n", extensionCount)

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
    app_info.apiVersion = VK_MAKE_API_VERSION(0, 1, 3, 0);

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
    
    create_info.enabledLayerCount = static_cast<unsigned>(VulkanEngineRequirements::validation_layers.size());
    create_info.ppEnabledLayerNames = VulkanEngineRequirements::validation_layers.data();

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
    VulkanEngineQueryStorage::queue_family_indices = FindQueueFamily(select_physical_device, surface);
    GLTF_CHECK(VulkanEngineQueryStorage::queue_family_indices.graphics_family.has_value());

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<unsigned> unique_queue_families =
        {
            VulkanEngineQueryStorage::queue_family_indices.graphics_family.value(),
            VulkanEngineQueryStorage::queue_family_indices.present_family.value()
        };
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
    //VkPhysicalDeviceFeatures device_features {};
    
    VkDeviceCreateInfo create_device_info{};
    create_device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_device_info.pQueueCreateInfos = queue_create_infos.data();
    create_device_info.queueCreateInfoCount = static_cast<unsigned>(queue_create_infos.size());
    //create_device_info.pEnabledFeatures = &device_features;
    create_device_info.ppEnabledLayerNames = VulkanEngineRequirements::validation_layers.data();
    create_device_info.enabledLayerCount = static_cast<unsigned>(VulkanEngineRequirements::validation_layers.size());
    create_device_info.ppEnabledExtensionNames = VulkanEngineRequirements::device_extensions.data();
    create_device_info.enabledExtensionCount = static_cast<unsigned>(VulkanEngineRequirements::device_extensions.size());
    create_device_info.pNext = &VulkanEngineQueryStorage::query_features2;
    
    result = vkCreateDevice(select_physical_device, &create_device_info, nullptr, &logical_device); 
    GLTF_CHECK(result == VK_SUCCESS);

    VmaAllocatorCreateInfo allocator_create_info {};
    allocator_create_info.physicalDevice = select_physical_device;
    allocator_create_info.device = logical_device;
    allocator_create_info.instance = instance;
    allocator_create_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocator_create_info, &vma_allocator);
    
    vkGetDeviceQueue(logical_device, VulkanEngineQueryStorage::queue_family_indices.graphics_family.value(), 0, &graphics_queue);
    vkGetDeviceQueue(logical_device, VulkanEngineQueryStorage::queue_family_indices.present_family.value(), 0, &present_queue);

    SwapChainSupportDetails detail = QuerySwapChainSupport(select_physical_device, surface);
    VkFormat color_attachment_format = chooseSwapChainSurfaceFormat(detail.formats).format;

    if (init_render_pass)
    {
        // Create render pass
        VkAttachmentDescription color_attachment {};
        color_attachment.format = color_attachment_format;
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_attachment_ref{};
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_ref;
    
        VkRenderPassCreateInfo create_render_pass_info{};
        create_render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        create_render_pass_info.attachmentCount = 1;
        create_render_pass_info.pAttachments = &color_attachment;
        create_render_pass_info.subpassCount = 1;
        create_render_pass_info.pSubpasses = &subpass;

        VkSubpassDependency dependency {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        create_render_pass_info.dependencyCount = 1;
        create_render_pass_info.pDependencies = &dependency;
    
        result = vkCreateRenderPass(logical_device, &create_render_pass_info, nullptr, &render_pass);
        GLTF_CHECK(result == VK_SUCCESS);
    }
    
    CreateSwapChainAndRelativeResource();

    //create a descriptor pool that will hold 10 sets with 1 image each
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
    {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
    };

    globalDescriptorAllocator.init_pool(logical_device, 10, sizes);

    //make the descriptor set layout for our compute draw
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _drawImageDescriptorLayout = builder.build(logical_device, VK_SHADER_STAGE_COMPUTE_BIT);
    }
    
    //allocate a descriptor set for our draw image
    _drawImageDescriptors = globalDescriptorAllocator.allocate(logical_device,_drawImageDescriptorLayout);	

    VkDescriptorImageInfo imgInfo{};
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imgInfo.imageView = draw_image.image_view;
	
    VkWriteDescriptorSet drawImageWrite = {};
    drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    drawImageWrite.pNext = nullptr;
	
    drawImageWrite.dstBinding = 0;
    drawImageWrite.dstSet = _drawImageDescriptors;
    drawImageWrite.descriptorCount = 1;
    drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    drawImageWrite.pImageInfo = &imgInfo;

    vkUpdateDescriptorSets(logical_device, 1, &drawImageWrite, 0, nullptr);
    
    if (run_graphics_test)
    {
        InitGraphicsPipeline();    
    }
    if (run_compute_test)
    {
        InitComputePipeline();
    }
    
    // Create command pool
    VkCommandPoolCreateInfo create_command_pool_info{};
    create_command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    create_command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    create_command_pool_info.queueFamilyIndex = VulkanEngineQueryStorage::queue_family_indices.graphics_family.value();

    result = vkCreateCommandPool(logical_device, &create_command_pool_info, nullptr, &command_pool);
    GLTF_CHECK(result == VK_SUCCESS);

    // Create command buffer
    command_buffers.resize(MAX_FRAMES_IN_FLIGHT);
    
    VkCommandBufferAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = command_pool;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = static_cast<unsigned>(command_buffers.size());

    result = vkAllocateCommandBuffers(logical_device, &allocate_info, command_buffers.data());
    GLTF_CHECK(result == VK_SUCCESS);

    // Create sync objects
    image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
    render_finish_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
    in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo create_semaphore_info{};
    create_semaphore_info.sType= VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo create_fence_info {};
    create_fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    create_fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        result = vkCreateSemaphore(logical_device, &create_semaphore_info, nullptr, &image_available_semaphores[i]);
        GLTF_CHECK(result == VK_SUCCESS);

        result = vkCreateSemaphore(logical_device, &create_semaphore_info, nullptr, &render_finish_semaphores[i]);
        GLTF_CHECK(result == VK_SUCCESS);

        result = vkCreateFence(logical_device, &create_fence_info, nullptr, &in_flight_fences[i]);
        GLTF_CHECK(result == VK_SUCCESS);
    }

    VK_CHECK(vkCreateCommandPool(logical_device, &create_command_pool_info, nullptr, &_immCommandPool));
    VkCommandBufferAllocateInfo cmd_allocator_info
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = _immCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VK_CHECK(vkAllocateCommandBuffers(logical_device, &cmd_allocator_info, &_immCommandBuffer));

    VK_CHECK(vkCreateFence(logical_device, &create_fence_info, nullptr, &_immFence));

    InitMeshBuffer();
    
    return true;
}

void VulkanEngine::RecordCommandBufferForDrawTestTriangle(VkCommandBuffer command_buffer, unsigned image_index)
{
    if (init_render_pass)
    {
        VkRenderPassBeginInfo render_pass_begin_info{};
        render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_begin_info.renderPass = render_pass;
        render_pass_begin_info.framebuffer = swap_chain_frame_buffers[image_index];
        render_pass_begin_info.renderArea.offset = {0, 0};
        render_pass_begin_info.renderArea.extent = swap_chain_extent;
        const VkClearValue clear_value = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        render_pass_begin_info.clearValueCount = 1;
        render_pass_begin_info.pClearValues = &clear_value;
        vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);    
    }
    else
    {
        VkRenderingAttachmentInfo color_attachment { .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        color_attachment.imageView = draw_image.image_view;
        color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_attachment.clearValue.color = {{1.0, 1.0 ,1.0, 1.0}};
        color_attachment.loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;

        VkRenderingInfo rendering_info {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO};
        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachments = &color_attachment;
        rendering_info.renderArea = {{0, 0}, {swap_chain_extent.width, swap_chain_extent.height}};
        rendering_info.layerCount = 1;
        
        vkCmdBeginRendering(command_buffer, &rendering_info);
    }
    
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);
    
    // since define viewport and scissor as dynamic state so do set in command
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swap_chain_extent.width);
    viewport.height = static_cast<float>(swap_chain_extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swap_chain_extent;
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    GPUDrawPushConstants push_constants{};
    push_constants.worldMatrix = glm::mat4{1.0f};
    push_constants.vertexBuffer = mesh_buffers.vertexBufferAddress;
    vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0 ,sizeof(GPUDrawPushConstants), &push_constants);
    vkCmdBindIndexBuffer(command_buffer, mesh_buffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    
    vkCmdDrawIndexed(command_buffer, 6, 1, 0, 0, 0);
    
    if (init_render_pass)
    {
        vkCmdEndRenderPass(command_buffer);
    }
    else
    {
        vkCmdEndRendering(command_buffer);
    }
}

void VulkanEngine::RecordCommandBufferForDynamicRendering(VkCommandBuffer command_buffer, unsigned image_index)
{
    VkImage current_draw_image = draw_image.image;

    float flash = std::abs(std::sin(current_frame_real / 1200.0f));
    VkClearColorValue clear_color_value {{0.0f, 1.0f - flash, flash, 1.0f}};
    VkImageSubresourceRange clear_range = GetVkSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
    vkCmdClearColorImage(command_buffer, current_draw_image, VK_IMAGE_LAYOUT_GENERAL, &clear_color_value, 1, &clear_range);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipelineLayout, 0, 1, &_drawImageDescriptors, 0, nullptr);

    vkCmdDispatch(command_buffer, std::ceil(swap_chain_extent.width / 16.0f), std::ceil(swap_chain_extent.height / 16.0f), 1);
}

void VulkanEngine::DrawFrame()
{
    vkWaitForFences(logical_device, 1, &in_flight_fences[current_frame_clipped], VK_TRUE, UINT64_MAX);
    
    unsigned image_index;
    VkResult result = vkAcquireNextImageKHR(logical_device, swap_chain, UINT64_MAX, image_available_semaphores[current_frame_clipped], VK_NULL_HANDLE, &image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || window_resized)
    {
        // SwapChain is no longer available
        RecreateSwapChain();
        window_resized = false;
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        // format error
        GLTF_CHECK(false);
    }

    VkCommandBuffer current_command_buffer = command_buffers[current_frame_clipped];
    VkImage current_swapchain_image = images[image_index];
    
    vkResetFences(logical_device, 1, &in_flight_fences[current_frame_clipped]);
    vkResetCommandBuffer(current_command_buffer, 0);

    // Create command buffer begin info
    VkCommandBufferBeginInfo command_buffer_begin_info{};
    command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.flags = 0;
    command_buffer_begin_info.pInheritanceInfo = nullptr;

    result = vkBeginCommandBuffer(current_command_buffer, &command_buffer_begin_info);
    GLTF_CHECK(result == VK_SUCCESS);

    VkImage current_draw_image = draw_image.image;
    
    if (run_compute_test)
    {
        TransitionImage(current_command_buffer, current_draw_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        RecordCommandBufferForDynamicRendering(current_command_buffer, image_index);
        TransitionImage(current_command_buffer, current_draw_image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    }
    if (run_graphics_test)
    {
        TransitionImage(current_command_buffer, current_draw_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        RecordCommandBufferForDrawTestTriangle(current_command_buffer, image_index);
        TransitionImage(current_command_buffer, current_draw_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);   
    }

    // Copy image to swapchain
    TransitionImage(current_command_buffer, current_swapchain_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CopyImageToImage(current_command_buffer, current_draw_image, current_swapchain_image, draw_extent, swap_chain_extent);
    TransitionImage(current_command_buffer, current_swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    result = vkEndCommandBuffer(current_command_buffer);
    GLTF_CHECK(result == VK_SUCCESS);
    
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = {image_available_semaphores[current_frame_clipped]};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &current_command_buffer;

    VkSemaphore signal_semaphores[] = {render_finish_semaphores[current_frame_clipped]};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;
    
    result = vkQueueSubmit(graphics_queue, 1, &submit_info, in_flight_fences[current_frame_clipped]);
    
    GLTF_CHECK(result == VK_SUCCESS); 

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;
    
    VkSwapchainKHR swap_chains[] = {swap_chain};
    present_info.pSwapchains = swap_chains;
    present_info.swapchainCount = 1;
    present_info.pImageIndices = &image_index;
    present_info.pResults = nullptr;
    
    result = vkQueuePresentKHR(present_queue, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || window_resized)
    {
        RecreateSwapChain();
        window_resized = false;
        return;
    }

    current_frame_real++;
    current_frame_clipped = (current_frame_clipped + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanEngine::CreateSwapChainAndRelativeResource()
{
    const QueueFamilyIndices queue_family_indices = FindQueueFamily(select_physical_device, surface);
    GLTF_CHECK(queue_family_indices.graphics_family.has_value());
    
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
    create_swap_chain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    
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

    VkResult result = vkCreateSwapchainKHR(logical_device, &create_swap_chain_info, nullptr, &swap_chain);
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

    // Create frame buffers
    if (init_render_pass)
    {
        swap_chain_frame_buffers.resize(image_views.size());

        for (size_t i = 0; i < image_views.size(); ++i)
        {
            VkImageView attachments[] = {image_views[i]};

            VkFramebufferCreateInfo framebuffer_create_info{};
            framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_create_info.renderPass = render_pass;
            framebuffer_create_info.attachmentCount = 1;
            framebuffer_create_info.pAttachments = attachments;
            framebuffer_create_info.width = swap_chain_extent.width;
            framebuffer_create_info.height = swap_chain_extent.height;
            framebuffer_create_info.layers = 1;

            result = vkCreateFramebuffer(logical_device, &framebuffer_create_info, nullptr, &swap_chain_frame_buffers[i]);
            GLTF_CHECK(result == VK_SUCCESS);
        }
    }

    draw_image.image_format = VK_FORMAT_R16G16B16A16_SFLOAT;
    draw_image.image_extent = {swap_chain_extent.width, swap_chain_extent.height, 1};

    VkImageUsageFlags draw_image_usages{};
    draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    draw_image_usages |= VK_IMAGE_USAGE_STORAGE_BIT;
    draw_image_usages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo image_create_info = GetImageCreateInfo(draw_image.image_format, draw_image_usages, draw_image.image_extent);
    VmaAllocationCreateInfo draw_image_allocation_create_info {};
    draw_image_allocation_create_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    draw_image_allocation_create_info.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vmaCreateImage(vma_allocator, &image_create_info, &draw_image_allocation_create_info, &draw_image.image, &draw_image.allocation, nullptr);
    VkImageViewCreateInfo image_view_create_info = GetImageViewCreateInfo(draw_image.image_format, draw_image.image, VK_IMAGE_ASPECT_COLOR_BIT);

    vkCreateImageView(logical_device, &image_view_create_info, nullptr, &draw_image.image_view);
    draw_extent = {draw_image.image_extent.width, draw_image.image_extent.height};
}

void VulkanEngine::CleanupSwapChain()
{
    vkDestroyImageView(logical_device, draw_image.image_view, nullptr);
    vmaDestroyImage(vma_allocator, draw_image.image, draw_image.allocation);

    if (init_render_pass)
    {
        for (const auto& frame_buffer:swap_chain_frame_buffers)
        {
            vkDestroyFramebuffer(logical_device, frame_buffer, nullptr);
        }    
    }

    for (const auto& image_view : image_views)
    {
        vkDestroyImageView(logical_device, image_view, nullptr);    
    }
    vkDestroySwapchainKHR(logical_device, swap_chain, nullptr);
}

void VulkanEngine::RecreateSwapChain()
{
    vkDeviceWaitIdle(logical_device);
    CleanupSwapChain();

    while (glTFWindow::Get().GetWidth() == 0 || glTFWindow::Get().GetHeight() == 0)
    {
        // Special case : minimization, wait for resize window
        Sleep(100);
        glfwWaitEvents();
    }
    
    CreateSwapChainAndRelativeResource();
}

void VulkanEngine::InitMeshBuffer()
{
    std::array<Vertex,4> rect_vertices;

    rect_vertices[0].position = {0.5,-0.5, 0};
    rect_vertices[1].position = {0.5,0.5, 0};
    rect_vertices[2].position = {-0.5,-0.5, 0};
    rect_vertices[3].position = {-0.5,0.5, 0};

    rect_vertices[0].color = {0,0, 0,1};
    rect_vertices[1].color = { 0.5,0.5,0.5 ,1};
    rect_vertices[2].color = { 1,0, 0,1 };
    rect_vertices[3].color = { 0,1, 0,1 };

    std::array<uint32_t,6> rect_indices;

    rect_indices[0] = 0;
    rect_indices[1] = 1;
    rect_indices[2] = 2;

    rect_indices[3] = 2;
    rect_indices[4] = 1;
    rect_indices[5] = 3;

    mesh_buffers = UploadMesh(rect_indices,rect_vertices);
}

void VulkanEngine::InitGraphicsPipeline()
{
    // Create shader module
    std::vector<unsigned char> vertex_shader_binaries;
    glTFShaderUtils::ShaderCompileDesc vertex_shader_compile_desc
    {
        "glTFResources/ShaderSource/GLSL/BufferRefVert.glsl",
        glTFShaderUtils::GetShaderCompileTarget(RHIShaderType::Vertex),
        "main",
        {},
        true,
        glTFShaderUtils::ShaderFileType::SFT_GLSL,
        glTFShaderUtils::ShaderType::ST_Vertex
    };
    glTFShaderUtils::CompileShader(vertex_shader_compile_desc, vertex_shader_binaries);

    std::vector<unsigned char> fragment_shader_binaries;
    glTFShaderUtils::ShaderCompileDesc fragment_shader_compile_desc
    {
        "glTFResources/ShaderSource/GLSL/SimpleFragShader.glsl",
        glTFShaderUtils::GetShaderCompileTarget(RHIShaderType::Pixel),
        "main",
        {},
        true,
        glTFShaderUtils::ShaderFileType::SFT_GLSL,
        glTFShaderUtils::ShaderType::ST_Fragment
    };
    glTFShaderUtils::CompileShader(fragment_shader_compile_desc, fragment_shader_binaries);

    auto vertex_shader_module = CreateShaderModule(logical_device, vertex_shader_binaries);
    auto fragment_shader_module = CreateShaderModule(logical_device, fragment_shader_binaries);

    VkPipelineShaderStageCreateInfo create_vertex_stage_info{};
    create_vertex_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    create_vertex_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    create_vertex_stage_info.module = vertex_shader_module;
    create_vertex_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo create_frag_stage_info{};
    create_frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    create_frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    create_frag_stage_info.module = fragment_shader_module;
    create_frag_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo shader_stages[] = {create_vertex_stage_info, create_frag_stage_info}; 

    VkPipelineVertexInputStateCreateInfo create_vertex_input_state_info {};
    create_vertex_input_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    create_vertex_input_state_info.vertexBindingDescriptionCount = 0;
    create_vertex_input_state_info.pVertexBindingDescriptions = nullptr;
    create_vertex_input_state_info.vertexAttributeDescriptionCount = 0;
    create_vertex_input_state_info.pVertexAttributeDescriptions = nullptr;

    VkPipelineInputAssemblyStateCreateInfo create_input_assembly_info {};
    create_input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    create_input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    create_input_assembly_info.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) swap_chain_extent.width;
    viewport.height = (float) swap_chain_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swap_chain_extent;

    std::vector<VkDynamicState> dynamic_states =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo create_dynamic_state_info {};
    create_dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    create_dynamic_state_info.dynamicStateCount = static_cast<unsigned>(dynamic_states.size());
    create_dynamic_state_info.pDynamicStates = dynamic_states.data();

    VkPipelineViewportStateCreateInfo create_viewport_state_info {};
    create_viewport_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    create_viewport_state_info.viewportCount = 1;
    create_viewport_state_info.scissorCount = 1;
    // dynamic state can switch viewport and scissor during draw commands, so no need to fill viewport and scissor now
    //create_viewport_state_info.pViewports = &viewport;
    //create_viewport_state_info.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo create_rasterizer_info{};
    create_rasterizer_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    create_rasterizer_info.depthClampEnable = VK_FALSE;
    create_rasterizer_info.rasterizerDiscardEnable = VK_FALSE;
    create_rasterizer_info.polygonMode = VK_POLYGON_MODE_FILL;
    create_rasterizer_info.lineWidth = 1.0f;
    create_rasterizer_info.cullMode = VK_CULL_MODE_BACK_BIT;
    create_rasterizer_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    create_rasterizer_info.depthBiasEnable = VK_FALSE;
    create_rasterizer_info.depthBiasConstantFactor = 0.0f;
    create_rasterizer_info.depthBiasClamp = 0.0f;
    create_rasterizer_info.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo create_msaa_state {};
    create_msaa_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    create_msaa_state.sampleShadingEnable = VK_FALSE;
    create_msaa_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    create_msaa_state.minSampleShading = 1.0f;
    create_msaa_state.pSampleMask = nullptr;
    create_msaa_state.alphaToCoverageEnable = VK_FALSE;
    create_msaa_state.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState color_blend_attachment_state{};
    color_blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment_state.blendEnable = VK_FALSE;
    color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo create_color_blend_state_info {};
    create_color_blend_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    create_color_blend_state_info.logicOpEnable = VK_FALSE;
    create_color_blend_state_info.logicOp = VK_LOGIC_OP_COPY;
    create_color_blend_state_info.attachmentCount = 1;
    create_color_blend_state_info.pAttachments = &color_blend_attachment_state;
    create_color_blend_state_info.blendConstants[0] = 0.0f;
    create_color_blend_state_info.blendConstants[1] = 0.0f;
    create_color_blend_state_info.blendConstants[2] = 0.0f;
    create_color_blend_state_info.blendConstants[3] = 0.0f;

    VkPipelineLayoutCreateInfo create_pipeline_layout_info {};
    create_pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    create_pipeline_layout_info.setLayoutCount = 0;
    create_pipeline_layout_info.pSetLayouts = nullptr;
    create_pipeline_layout_info.pushConstantRangeCount = 0;
    create_pipeline_layout_info.pPushConstantRanges = nullptr;

    VkPushConstantRange push_constant_range;
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(GPUDrawPushConstants);
    push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    create_pipeline_layout_info.pPushConstantRanges = &push_constant_range;
    create_pipeline_layout_info.pushConstantRangeCount = 1;

    VkResult result = vkCreatePipelineLayout(logical_device, &create_pipeline_layout_info, nullptr, &pipeline_layout);
    GLTF_CHECK(result == VK_SUCCESS);

    VkPipelineRenderingCreateInfo pipeline_rendering_create_info {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    pipeline_rendering_create_info.colorAttachmentCount = 1;
    pipeline_rendering_create_info.pColorAttachmentFormats = &draw_image.image_format;
    pipeline_rendering_create_info.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    
    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo create_graphics_pipeline_info {};
    create_graphics_pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_graphics_pipeline_info.stageCount = 2;
    create_graphics_pipeline_info.pStages = shader_stages;
    create_graphics_pipeline_info.pVertexInputState = &create_vertex_input_state_info;
    create_graphics_pipeline_info.pInputAssemblyState = &create_input_assembly_info;
    create_graphics_pipeline_info.pViewportState = &create_viewport_state_info;
    create_graphics_pipeline_info.pRasterizationState = &create_rasterizer_info;
    create_graphics_pipeline_info.pMultisampleState = &create_msaa_state;
    create_graphics_pipeline_info.pDepthStencilState = nullptr;
    create_graphics_pipeline_info.pColorBlendState = &create_color_blend_state_info;
    create_graphics_pipeline_info.pDynamicState = &create_dynamic_state_info;
    create_graphics_pipeline_info.layout = pipeline_layout;
    if (init_render_pass)
    {
        create_graphics_pipeline_info.renderPass = render_pass;    
    }
    create_graphics_pipeline_info.subpass = 0;
    create_graphics_pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    create_graphics_pipeline_info.basePipelineIndex = -1;
    create_graphics_pipeline_info.pNext = &pipeline_rendering_create_info;
    
    result = vkCreateGraphicsPipelines(logical_device, VK_NULL_HANDLE, 1, &create_graphics_pipeline_info, nullptr, &graphics_pipeline);
    GLTF_CHECK(result == VK_SUCCESS);

    vkDestroyShaderModule(logical_device, fragment_shader_module, nullptr);
    vkDestroyShaderModule(logical_device, vertex_shader_module, nullptr);
}

void VulkanEngine::InitComputePipeline()
{
    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pNext = nullptr;
    computeLayout.pSetLayouts = &_drawImageDescriptorLayout;
    computeLayout.setLayoutCount = 1;
    
    VK_CHECK(vkCreatePipelineLayout(logical_device, &computeLayout, nullptr, &_gradientPipelineLayout));
    
    // Create shader module
    std::vector<unsigned char> compute_shader_binaries;
    /*
    glTFShaderUtils::ShaderCompileDesc compute_shader_compile_desc
    {
        "glTFResources/ShaderSource/TestShaders/TestGradientCompute.hlsl",
        glTFShaderUtils::GetShaderCompileTarget(RHIShaderType::Compute),
        "main",
        {},
        true
    };
    glTFShaderUtils::CompileShader(compute_shader_compile_desc, compute_shader_binaries);
    */
    glTFShaderUtils::ShaderCompileDesc compute_shader_compile_desc
    {
        "glTFResources/ShaderSource/GLSL/SimpleComputeShader.glsl",
        glTFShaderUtils::GetShaderCompileTarget(RHIShaderType::Compute),
        "main",
        {},
        true,
        glTFShaderUtils::ShaderFileType::SFT_GLSL,
        glTFShaderUtils::ShaderType::ST_Compute
    };
    
    glTFShaderUtils::CompileShader(compute_shader_compile_desc, compute_shader_binaries);
    auto compute_shader_module = CreateShaderModule(logical_device, compute_shader_binaries);

    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.pNext = nullptr;
    stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageinfo.module = compute_shader_module;
    stageinfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = _gradientPipelineLayout;
    computePipelineCreateInfo.stage = stageinfo;
	
    VK_CHECK(vkCreateComputePipelines(logical_device, VK_NULL_HANDLE,1, &computePipelineCreateInfo, nullptr, &compute_pipeline));

    vkDestroyShaderModule(logical_device, compute_shader_module, nullptr);
}

bool VulkanEngine::Update()
{
    DrawFrame();
    return true;
}

bool VulkanEngine::UnInit()
{
    vkDeviceWaitIdle(logical_device);

    vkDestroyFence(logical_device, _immFence, nullptr);
    vkDestroyCommandPool(logical_device, _immCommandPool, nullptr);
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        vkDestroySemaphore(logical_device, image_available_semaphores[i], nullptr);
        vkDestroySemaphore(logical_device, render_finish_semaphores[i], nullptr);
        vkDestroyFence(logical_device, in_flight_fences[i], nullptr);
    }

    // No need to free command buffer when command pool is freed
    vkDestroyCommandPool(logical_device, command_pool, nullptr);

    CleanupSwapChain();

    DestroyBuffer(mesh_buffers.vertexBuffer);
    DestroyBuffer(mesh_buffers.indexBuffer);
    
    vmaDestroyAllocator(vma_allocator);

    if (init_render_pass)
    {
        vkDestroyRenderPass(logical_device, render_pass, nullptr);
    }
    
    if (run_graphics_test)
    {
        vkDestroyPipeline(logical_device, graphics_pipeline, nullptr);
        vkDestroyPipelineLayout(logical_device, pipeline_layout, nullptr);    
    }
    
    if (run_compute_test)
    {
        vkDestroyPipeline(logical_device, compute_pipeline, nullptr);
        vkDestroyPipelineLayout(logical_device, _gradientPipelineLayout, nullptr);   
    }
    
    globalDescriptorAllocator.destroy_pool(logical_device);
    vkDestroyDescriptorSetLayout(logical_device, _drawImageDescriptorLayout, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyDevice(logical_device, nullptr);
    vkDestroyInstance(instance, nullptr);    
    
    return true;
}

void VulkanEngine::ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
{
    VK_CHECK(vkResetFences(logical_device, 1, &_immFence));
    VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

    VkCommandBuffer cmd = _immCommandBuffer;
    VkCommandBufferBeginInfo cmdBeginInfo
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = nullptr,
        .commandBuffer = cmd,
        .deviceMask = 0
    };
    
    VkSubmitInfo2 submit
    {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdinfo,
    };

    // submit command buffer to the queue and execute it.
    //  _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(graphics_queue, 1, &submit, _immFence));

    VK_CHECK(vkWaitForFences(logical_device, 1, &_immFence, true, 9999999999));
}