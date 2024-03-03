#include "VKSemaphore.h"

#include "VKDevice.h"

VKSemaphore::~VKSemaphore()
{
    vkDestroySemaphore(m_device, m_semaphore, nullptr);
}

bool VKSemaphore::InitSemaphore(IRHIDevice& device)
{
    m_device = dynamic_cast<VKDevice&>(device).GetDevice();
    
    VkSemaphoreCreateInfo create_semaphore_info{};
    create_semaphore_info.sType= VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    const VkResult result = vkCreateSemaphore(m_device, &create_semaphore_info, nullptr, &m_semaphore);
    GLTF_CHECK(result == VK_SUCCESS);

    return true;
}

VkSemaphore VKSemaphore::GetSemaphore() const
{
    return m_semaphore;
}
