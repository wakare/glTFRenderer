#include "VKCommandList.h"

#include "VKDevice.h"

VKCommandList::~VKCommandList()
{
    // Nothing to do
}

bool VKCommandList::InitCommandList(IRHIDevice& device, IRHICommandAllocator& command_allocator)
{
    const VkCommandPool vk_command_pool = dynamic_cast<VKCommandAllocator&>(command_allocator).GetCommandPool();
    const VkDevice vk_device = dynamic_cast<VKDevice&>(device).GetDevice();
    
    VkCommandBufferAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = vk_command_pool;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = 1;

    const VkResult result = vkAllocateCommandBuffers(vk_device, &allocate_info, &m_command_buffer);
    GLTF_CHECK(result == VK_SUCCESS);

    return true;
}
