#include "glTFVulkanTest.h"

#include <optional>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <algorithm>
#include <set>

#include "glTFShaderUtils/glTFShaderUtils.h"
#include "glTFWindow/glTFWindow.h"

const int MAX_FRAMES_IN_FLIGHT = 2;

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

bool glTFVulkanTest::Init()
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

    SwapChainSupportDetails detail = QuerySwapChainSupport(select_physical_device, surface);
    VkFormat color_attachment_format = chooseSwapChainSurfaceFormat(detail.formats).format;
    
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
    
    CreateSwapChainAndRelativeResource();

    // Create shader module
    std::vector<unsigned char> vertex_shader_binaries;
    glTFShaderUtils::ShaderCompileDesc vertex_shader_compile_desc
    {
        "glTFResources/ShaderSource/TestShaders/TestTriangleVert.hlsl",
        glTFShaderUtils::GetShaderCompileTarget(RHIShaderType::Vertex),
        "main",
        {},
        true
    };
    glTFShaderUtils::CompileShader(vertex_shader_compile_desc, vertex_shader_binaries);

    std::vector<unsigned char> fragment_shader_binaries;
    glTFShaderUtils::ShaderCompileDesc fragment_shader_compile_desc
    {
        "glTFResources/ShaderSource/TestShaders/TestTriangleFrag.hlsl",
        glTFShaderUtils::GetShaderCompileTarget(RHIShaderType::Pixel),
        "main",
        {},
        true
    };
    glTFShaderUtils::CompileShader(fragment_shader_compile_desc, fragment_shader_binaries);

    vertex_shader_module = CreateShaderModule(logical_device, vertex_shader_binaries);
    fragment_shader_module = CreateShaderModule(logical_device, fragment_shader_binaries);

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

    result = vkCreatePipelineLayout(logical_device, &create_pipeline_layout_info, nullptr, &pipeline_layout);
    GLTF_CHECK(result == VK_SUCCESS);

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
    create_graphics_pipeline_info.renderPass = render_pass;
    create_graphics_pipeline_info.subpass = 0;
    create_graphics_pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    create_graphics_pipeline_info.basePipelineIndex = -1;

    result = vkCreateGraphicsPipelines(logical_device, VK_NULL_HANDLE, 1, &create_graphics_pipeline_info, nullptr, &pipeline);
    GLTF_CHECK(result == VK_SUCCESS);

    // Create command pool
    VkCommandPoolCreateInfo create_command_pool_info{};
    create_command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    create_command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    create_command_pool_info.queueFamilyIndex = queue_family_indices.graphics_family.value();

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
    
    return true;
}

void glTFVulkanTest::RecordCommandBuffer(VkCommandBuffer command_buffer, unsigned image_index)
{
    // Create command buffer begin info
    VkCommandBufferBeginInfo command_buffer_begin_info{};
    command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.flags = 0;
    command_buffer_begin_info.pInheritanceInfo = nullptr;

    VkResult result = vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);
    GLTF_CHECK(result == VK_SUCCESS);

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
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    
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

    vkCmdDraw(command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);
    
    result = vkEndCommandBuffer(command_buffer);
    
    GLTF_CHECK(result == VK_SUCCESS);
}

void glTFVulkanTest::DrawFrame()
{
    vkWaitForFences(logical_device, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);
    
    unsigned image_index;
    VkResult result = vkAcquireNextImageKHR(logical_device, swap_chain, UINT64_MAX, image_available_semaphores[current_frame], VK_NULL_HANDLE, &image_index);
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

    vkResetFences(logical_device, 1, &in_flight_fences[current_frame]);
    
    vkResetCommandBuffer(command_buffers[current_frame], 0);

    RecordCommandBuffer(command_buffers[current_frame], image_index);
    
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = {image_available_semaphores[current_frame]};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffers[current_frame];

    VkSemaphore signal_semaphores[] = {render_finish_semaphores[current_frame]};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;
    
    result = vkQueueSubmit(graphics_queue, 1, &submit_info, in_flight_fences[current_frame]);
    
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
    
    current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void glTFVulkanTest::CreateSwapChainAndRelativeResource()
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

void glTFVulkanTest::CleanupSwapChain()
{
    for (const auto& frame_buffer:swap_chain_frame_buffers)
    {
        vkDestroyFramebuffer(logical_device, frame_buffer, nullptr);
    }

    for (const auto& image_view : image_views)
    {
        vkDestroyImageView(logical_device, image_view, nullptr);    
    }
    vkDestroySwapchainKHR(logical_device, swap_chain, nullptr);
}

void glTFVulkanTest::RecreateSwapChain()
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

bool glTFVulkanTest::Update()
{
    DrawFrame();
    return true;
}

bool glTFVulkanTest::UnInit()
{
    vkDeviceWaitIdle(logical_device);
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        vkDestroySemaphore(logical_device, image_available_semaphores[i], nullptr);
        vkDestroySemaphore(logical_device, render_finish_semaphores[i], nullptr);
        vkDestroyFence(logical_device, in_flight_fences[i], nullptr);
    }

    // No need to free command buffer when command pool is freed
    vkDestroyCommandPool(logical_device, command_pool, nullptr);

    CleanupSwapChain();
    
    vkDestroyPipeline(logical_device, pipeline, nullptr);
    vkDestroyRenderPass(logical_device, render_pass, nullptr);
    vkDestroyPipelineLayout(logical_device, pipeline_layout, nullptr);
    vkDestroyShaderModule(logical_device, fragment_shader_module, nullptr);
    vkDestroyShaderModule(logical_device, vertex_shader_module, nullptr);
    
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyDevice(logical_device, nullptr);
    vkDestroyInstance(instance, nullptr);    
    
    return true;
}
