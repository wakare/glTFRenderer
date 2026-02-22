#include "VKSwapChain.h"

#include <algorithm>

#include "VKCommandList.h"
#include "VKCommandQueue.h"
#include "VKCommon.h"
#include "VKConverterUtils.h"
#include "VKDevice.h"
#include "VKSemaphore.h"
#include "VKTexture.h"
#include "RHIResourceFactoryImpl.hpp"

unsigned VKSwapChain::GetCurrentBackBufferIndex()
{
    return m_current_frame_index;
}

unsigned VKSwapChain::GetCurrentSwapchainImageIndex()
{
    return m_image_index;
}

unsigned VKSwapChain::GetBackBufferCount()
{
    if (!m_swap_chain_textures.empty())
    {
        return static_cast<unsigned>(m_swap_chain_textures.size());
    }
    return m_frame_buffer_count;
}

bool VKSwapChain::InitSwapChain(IRHIFactory& factory, IRHIDevice& device, IRHICommandQueue& commandQueue, const RHITextureDesc& swap_chain_buffer_desc, const
                                RHISwapChainDesc& swap_chain_desc)
{
    // Reinitialize per-swapchain state to avoid stale present-id waits after runtime resize/recreate.
    m_current_frame_index = 0;
    m_image_index = 0;
    m_present_id_count = 0;
    m_swap_chain_textures.clear();
    m_frame_available_semaphores.clear();
    m_frame_semaphore_wait_infos.clear();

    m_swap_chain_buffer_desc = swap_chain_buffer_desc;
    m_swap_chain_mode = swap_chain_desc.chain_mode;

    const VKDevice& VkDevice = dynamic_cast<VKDevice&>(device);
    m_device = VkDevice.GetDevice();
    
    const VkSurfaceKHR surface = VkDevice.GetSurface();
    const VkPhysicalDevice physical_device = VkDevice.GetPhysicalDevice();
    const unsigned graphics_queue_index = VkDevice.GetGraphicsQueueIndex();
    const unsigned present_queue_index = VkDevice.GetPresentQueueIndex();

    VkSurfaceCapabilitiesKHR surface_capabilities{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities));

    VkExtent2D swapchain_extent{};
    if (surface_capabilities.currentExtent.width != UINT32_MAX &&
        surface_capabilities.currentExtent.height != UINT32_MAX)
    {
        swapchain_extent = surface_capabilities.currentExtent;
    }
    else
    {
        swapchain_extent.width = (std::max)(surface_capabilities.minImageExtent.width,
            (std::min)(surface_capabilities.maxImageExtent.width, GetWidth()));
        swapchain_extent.height = (std::max)(surface_capabilities.minImageExtent.height,
            (std::min)(surface_capabilities.maxImageExtent.height, GetHeight()));
    }

    uint32_t swapchain_image_count = m_frame_buffer_count;
    if (swapchain_image_count < surface_capabilities.minImageCount)
    {
        swapchain_image_count = surface_capabilities.minImageCount;
    }
    if (surface_capabilities.maxImageCount > 0 && swapchain_image_count > surface_capabilities.maxImageCount)
    {
        swapchain_image_count = surface_capabilities.maxImageCount;
    }

    m_swap_chain_buffer_desc = RHITextureDesc(
        m_swap_chain_buffer_desc.GetName(),
        swapchain_extent.width,
        swapchain_extent.height,
        m_swap_chain_buffer_desc.GetDataFormat(),
        m_swap_chain_buffer_desc.GetUsage(),
        m_swap_chain_buffer_desc.GetClearValue());
    
    VkSwapchainCreateInfoKHR create_swap_chain_info {};
    create_swap_chain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_swap_chain_info.surface = surface;
    create_swap_chain_info.minImageCount = swapchain_image_count;
    create_swap_chain_info.imageFormat = VKConverterUtils::ConvertToFormat(GetBackBufferFormat());
    create_swap_chain_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    create_swap_chain_info.imageExtent = swapchain_extent;
    create_swap_chain_info.imageArrayLayers = 1;
    
    if (m_swap_chain_buffer_desc.GetUsage() & RUF_ALLOW_RENDER_TARGET)
    {
        create_swap_chain_info.imageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;    
    }
    
    if (m_swap_chain_buffer_desc.GetUsage() & RUF_ALLOW_UAV)
    {
        create_swap_chain_info.imageUsage |= VK_IMAGE_USAGE_STORAGE_BIT;    
    }

    if (m_swap_chain_buffer_desc.GetUsage() & RUF_TRANSFER_DST)
    {
        create_swap_chain_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    
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
    switch (m_swap_chain_mode) {
    case VSYNC:
        create_swap_chain_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        break;
    case MAILBOX:
        create_swap_chain_info.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
        break;
    }
    
    create_swap_chain_info.clipped = VK_TRUE;
    create_swap_chain_info.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(VkDevice.GetDevice(), &create_swap_chain_info, nullptr, &m_swap_chain);
    GLTF_CHECK(result == VK_SUCCESS);

    unsigned swap_chain_image_count = 0;
    vkGetSwapchainImagesKHR(VkDevice.GetDevice(), m_swap_chain, &swap_chain_image_count, nullptr);
    if (swap_chain_image_count == 0)
    {
        return false;
    }

    m_frame_buffer_count = swap_chain_image_count;
    
    if (swap_chain_image_count)
    {
        m_swap_chain_textures.resize(swap_chain_image_count);
        std::vector<VkImage> internal_textures(swap_chain_image_count);
        vkGetSwapchainImagesKHR(VkDevice.GetDevice(), m_swap_chain, &swap_chain_image_count, internal_textures.data());

        for (size_t i = 0; i < swap_chain_image_count; ++i)
        {
            std::shared_ptr<IRHITexture> texture = RHIResourceFactory::CreateRHIResource<IRHITexture>();
            dynamic_cast<VKTexture&>(*texture).Init(VkDevice.GetDevice(), internal_textures[i], m_swap_chain_buffer_desc);
            m_swap_chain_textures[i] = texture;
        }
        
        m_frame_available_semaphores.resize(swap_chain_image_count);
        for (size_t i = 0; i < swap_chain_image_count; ++i)
        {
            const auto semaphore = RHIResourceFactory::CreateRHIResource<IRHISemaphore>();
            semaphore->InitSemaphore(device);
            m_frame_available_semaphores[i] = semaphore;
        }
    }
    
    need_release = true;
    
    return true;
}

bool VKSwapChain::AcquireNewFrame(IRHIDevice& device)
{
    if (m_frame_available_semaphores.empty())
    {
        return false;
    }

    const VKDevice& VkDevice = dynamic_cast<VKDevice&>(device);
    const VkSemaphore current_vk_semaphore = dynamic_cast<const VKSemaphore&>(GetAvailableFrameSemaphore()).GetSemaphore();

    const VkResult result = vkAcquireNextImageKHR(VkDevice.GetDevice(), m_swap_chain, UINT64_MAX, current_vk_semaphore, VK_NULL_HANDLE, &m_image_index);
    if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
    {
        return true;
    }
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return false;
    }

    LOG_FORMAT_FLUSH("[VKSwapChain] vkAcquireNextImageKHR failed: %d\n", static_cast<int>(result));
    return false;
}

IRHISemaphore& VKSwapChain::GetAvailableFrameSemaphore()
{
    GLTF_CHECK(!m_frame_available_semaphores.empty());
    const auto frame_index = m_current_frame_index % static_cast<unsigned>(m_frame_available_semaphores.size());
    return *m_frame_available_semaphores[frame_index];
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
    present_info.pImageIndices = &m_image_index;
    present_info.pResults = nullptr;

    VkPresentIdKHR present_id{};
    present_id.sType = VK_STRUCTURE_TYPE_PRESENT_ID_KHR;
    present_id.pNext = nullptr;
    present_id.swapchainCount = 1;
    
    m_present_id_count++;
    present_id.pPresentIds = &m_present_id_count;
    
    present_info.pNext = &present_id;

    const VkResult result = vkQueuePresentKHR(vk_command_queue.GetGraphicsQueue(), &present_info);
    if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
    {
        if (!m_frame_available_semaphores.empty())
        {
            m_current_frame_index = (m_current_frame_index + 1) % static_cast<unsigned>(m_frame_available_semaphores.size());
        }
        return true;
    }
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return false;
    }

    LOG_FORMAT_FLUSH("[VKSwapChain] vkQueuePresentKHR failed: %d\n", static_cast<int>(result));
    return false;
}

bool VKSwapChain::HostWaitPresentFinished(IRHIDevice& device)
{
    if (m_swap_chain == VK_NULL_HANDLE || m_present_id_count == 0)
    {
        return true;
    }

    auto vk_device = dynamic_cast<VKDevice&>(device).GetDevice();
    VK_CHECK(vkWaitForPresentKHR(vk_device, m_swap_chain, m_present_id_count, UINT64_MAX))
    
    return true;
}

bool VKSwapChain::Release(IRHIMemoryManager& memory_manager)
{
    if (!need_release)
    {
        return true;
    }
    
    need_release = false;
    vkDestroySwapchainKHR(m_device, m_swap_chain, nullptr);

    return true;
}

std::shared_ptr<IRHITexture> VKSwapChain::GetSwapChainImageByIndex(unsigned index) const
{
    GLTF_CHECK(index < m_swap_chain_textures.size());
    return m_swap_chain_textures[index];
}
