#include "VKCommandList.h"

#include "VKDevice.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "glTFRHI/RHIInterface/IRHIFence.h"
#include "VKCommandAllocator.h"

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

    m_fence = RHIResourceFactory::CreateRHIResource<IRHIFence>();
    m_fence->InitFence(device);

    m_finished_semaphore = RHIResourceFactory::CreateRHIResource<IRHISemaphore>();
    m_finished_semaphore->InitSemaphore(device);
    
    return true;
}

bool VKCommandList::WaitCommandList()
{
    if (m_fence->CanWait())
    {
        m_fence->HostWaitUtilSignaled();
        m_fence->ResetFence();    
    }
    
    return true;
}

bool VKCommandList::BeginRecordCommandList()
{
    VkCommandBufferBeginInfo command_buffer_begin_info{};
    command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.flags = 0;
    command_buffer_begin_info.pInheritanceInfo = nullptr;

    const VkResult result = vkBeginCommandBuffer(m_command_buffer, &command_buffer_begin_info);
    GLTF_CHECK(result == VK_SUCCESS);

    return true;
}

bool VKCommandList::EndRecordCommandList()
{
    const VkResult result = vkEndCommandBuffer(m_command_buffer);
    GLTF_CHECK(result == VK_SUCCESS);
    
    return true;
}
