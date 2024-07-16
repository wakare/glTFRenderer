#include "VKCommandAllocator.h"
#include "VKDevice.h"

VKCommandAllocator::~VKCommandAllocator()
{
    vkDestroyCommandPool(m_device, m_command_pool, nullptr);
}

bool VKCommandAllocator::InitCommandAllocator(IRHIDevice& device, RHICommandAllocatorType type)
{
    m_device = dynamic_cast<VKDevice&>(device).GetDevice();
    
    // Create command pool
    VkCommandPoolCreateInfo create_command_pool_info{};
    create_command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    create_command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    create_command_pool_info.queueFamilyIndex = dynamic_cast<VKDevice&>(device).GetGraphicsQueueIndex();

    const VkResult result = vkCreateCommandPool(m_device, &create_command_pool_info, nullptr, &m_command_pool);
    GLTF_CHECK(result == VK_SUCCESS);
    
    return true;
}

const VkCommandPool& VKCommandAllocator::GetCommandPool() const
{
    return m_command_pool;
}
