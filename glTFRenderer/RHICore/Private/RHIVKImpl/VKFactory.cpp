#include "VKFactory.h"
#include "RendererCommon.h"
#include <GLFW/glfw3.h>

const std::vector validation_layers =
{
    "VK_LAYER_KHRONOS_validation"
};

bool VKFactory::InitFactory()
{
    // Create vulkan instance
    VkApplicationInfo app_info{};
    app_info.sType = VkStructureType::VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "glTFRenderer (Vulkan)";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_MAKE_VERSION(1, 3, 0);

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

    VkResult result = vkCreateInstance(&create_info, nullptr, &m_instance);
    GLTF_CHECK(result == VK_SUCCESS);

    VolkUtils::LoadInstance(m_instance);
    need_release = true;
    
    return true;
}

bool VKFactory::Release(IRHIMemoryManager& memory_manager)
{
    if (!need_release)
    {
        return true;
    }

    need_release = false;
    vkDestroyInstance(m_instance, nullptr);
    
    return true;
}
