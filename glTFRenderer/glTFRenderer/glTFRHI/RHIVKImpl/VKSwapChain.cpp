#include "VKSwapChain.h"

#include "VKCommandList.h"
#include "VKCommandQueue.h"
#include "VKConverterUtils.h"
#include "VKDevice.h"
#include "VKSemaphore.h"

VKSwapChain::VKSwapChain()
    : m_frame_buffer_count(3)
{
}

VKSwapChain::~VKSwapChain()
{
    vkDestroySwapchainKHR(m_device, m_swap_chain, nullptr);
}

unsigned VKSwapChain::GetWidth() const
{
    return m_width;
}

unsigned VKSwapChain::GetHeight() const
{
    return m_height;
}

unsigned VKSwapChain::GetCurrentBackBufferIndex()
{
    return m_current_frame_index;
}

unsigned VKSwapChain::GetBackBufferCount()
{
    return m_frame_buffer_count;
}

bool VKSwapChain::InitSwapChain(IRHIFactory& factory, IRHIDevice& device, IRHICommandQueue& commandQueue, unsigned width, unsigned height,
                                bool fullScreen, HWND hwnd)
{
    const VKDevice& VkDevice = dynamic_cast<VKDevice&>(device);
    m_device = VkDevice.GetDevice();
    
    const VkSurfaceKHR surface = VkDevice.GetSurface();
    const unsigned graphics_queue_index = VkDevice.GetGraphicsQueueIndex();
    const unsigned present_queue_index = VkDevice.GetPresentQueueIndex();
    
    VkSwapchainCreateInfoKHR create_swap_chain_info {};
    create_swap_chain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_swap_chain_info.surface = surface;
    create_swap_chain_info.minImageCount = m_frame_buffer_count;
    create_swap_chain_info.imageFormat = VKConverterUtils::ConvertToFormat(m_back_buffer_format);
    create_swap_chain_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    create_swap_chain_info.imageExtent = {width, height};
    create_swap_chain_info.imageArrayLayers = 1;
    create_swap_chain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    if (graphics_queue_index != present_queue_index)
    {
        unsigned family_indices[] = {graphics_queue_index, present_queue_index}; 
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

    //create_swap_chain_info.preTransform = detail.capabilities.currentTransform;
    create_swap_chain_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    create_swap_chain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_swap_chain_info.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    create_swap_chain_info.clipped = VK_TRUE;
    create_swap_chain_info.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(VkDevice.GetDevice(), &create_swap_chain_info, nullptr, &m_swap_chain);
    GLTF_CHECK(result == VK_SUCCESS);

    unsigned swap_chain_image_count = 0;
    vkGetSwapchainImagesKHR(VkDevice.GetDevice(), m_swap_chain, &swap_chain_image_count, nullptr);
    
    if (swap_chain_image_count)
    {
        m_swap_chain_images.resize(swap_chain_image_count);
        vkGetSwapchainImagesKHR(VkDevice.GetDevice(), m_swap_chain, &swap_chain_image_count, m_swap_chain_images.data());

        m_frame_available_semaphores.resize(swap_chain_image_count);
        for (size_t i = 0; i < swap_chain_image_count; ++i)
        {
            const auto semaphore = std::make_shared<VKSemaphore>();
            semaphore->InitSemaphore(device);
            m_frame_available_semaphores[i] = semaphore;
        }
    }
    
    return true;
}

bool VKSwapChain::AcquireNewFrame(IRHIDevice& device)
{
    const VKDevice& VkDevice = dynamic_cast<VKDevice&>(device);
    const VkSemaphore current_vk_semaphore = dynamic_cast<const VKSemaphore&>(GetAvailableFrameSemaphore()).GetSemaphore();
    const VkResult result = vkAcquireNextImageKHR(VkDevice.GetDevice(), m_swap_chain, UINT64_MAX, current_vk_semaphore, VK_NULL_HANDLE, &m_current_frame_index);
    GLTF_CHECK(result == VK_SUCCESS);
    
    return true;
}

IRHISemaphore& VKSwapChain::GetAvailableFrameSemaphore()
{
    return *m_frame_available_semaphores[m_current_frame_index];
}

bool VKSwapChain::Present(IRHICommandQueue& command_queue, IRHICommandList& command_list)
{
    auto& vk_command_queue = dynamic_cast<VKCommandQueue&>(command_queue);
    auto& command_render_finished_semaphore = dynamic_cast<VKCommandList&>(command_list).GetSemaphore();
    auto vk_semaphore = dynamic_cast<VKSemaphore&>(command_render_finished_semaphore).GetSemaphore();
    
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    // TODO: semaphore?
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &vk_semaphore;
    
    VkSwapchainKHR swap_chains[] = {m_swap_chain};
    present_info.pSwapchains = swap_chains;
    present_info.swapchainCount = 1;
    present_info.pImageIndices = &m_current_frame_index;
    present_info.pResults = nullptr;

    const VkResult result = vkQueuePresentKHR(vk_command_queue.GetGraphicsQueue(), &present_info);
    GLTF_CHECK(result == VK_SUCCESS);
    
    return true;
}

VkImage VKSwapChain::GetSwapChainImageByIndex(unsigned index) const
{
    GLTF_CHECK(index < m_swap_chain_images.size());
    return m_swap_chain_images[index];
}
