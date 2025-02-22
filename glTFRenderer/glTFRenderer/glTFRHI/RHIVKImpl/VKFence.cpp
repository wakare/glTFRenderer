#include "VKFence.h"

#include "VKCommandQueue.h"

bool VKFence::InitFence(IRHIDevice& device)
{
    m_device = dynamic_cast<VKDevice&>(device).GetDevice();
    
    VkFenceCreateInfo create_fence_info {};
    create_fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    create_fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    const VkResult result = vkCreateFence(m_device, &create_fence_info, nullptr, &m_fence);
    GLTF_CHECK(result == VK_SUCCESS);

    SetCanWait(true);
    need_release = true;
    
    return true;
}

bool VKFence::HostWaitUtilSignaled()
{
    GLTF_CHECK(CanWait());
    
    const VkResult result = vkWaitForFences(m_device, 1, &m_fence, VK_TRUE, UINT64_MAX);
    GLTF_CHECK(result == VK_SUCCESS);
    return true;
}

bool VKFence::ResetFence()
{
    SetCanWait(false);
    
    const VkResult result = vkResetFences(m_device, 1, &m_fence);
    GLTF_CHECK(result == VK_SUCCESS);
    return true;
}

bool VKFence::Release(glTFRenderResourceManager&)
{
    if (!need_release)
    {
        return true;
    }

    need_release = false;
    vkDestroyFence(m_device, m_fence, nullptr);
    
    return true;
}
