#include "VKFrameBuffer.h"

#include "VKDevice.h"
#include "VKRenderPass.h"
#include "VKRenderTarget.h"
#include "VKSwapChain.h"

bool VKFrameBuffer::InitFrameBuffer(IRHIDevice& device, IRHISwapChain& swap_chain, const RHIFrameBufferInfo& info)
{
    m_device = dynamic_cast<VKDevice&>(device).GetDevice();
    const VkRenderPass vk_render_pass = dynamic_cast<VKRenderPass&>(*info.render_pass).GetRenderPass();
    
    std::vector<VkImageView> attachments(info.attachment_images.size());
    for (size_t i = 0; i < attachments.size(); ++i)
    {
        attachments.push_back(dynamic_cast<VKRenderTarget&>(*info.attachment_images[i]).GetImageView());
    }
    
    VkFramebufferCreateInfo framebuffer_create_info{};
    framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_create_info.renderPass = vk_render_pass;
    framebuffer_create_info.attachmentCount = attachments.size();
    framebuffer_create_info.pAttachments = attachments.data();
    framebuffer_create_info.width = dynamic_cast<VKSwapChain&>(swap_chain).GetWidth();
    framebuffer_create_info.height = dynamic_cast<VKSwapChain&>(swap_chain).GetHeight();
    framebuffer_create_info.layers = 1;

    const VkResult result = vkCreateFramebuffer(m_device, &framebuffer_create_info, nullptr, &m_frame_buffer);
    GLTF_CHECK(result == VK_SUCCESS);

    need_release = true;
    
    return true;
}

const VkFramebuffer& VKFrameBuffer::GetFrameBuffer() const
{
    return m_frame_buffer;
}

bool VKFrameBuffer::Release(IRHIMemoryManager& memory_manager)
{
    if (!need_release)
    {
        return true;
    }

    need_release = false;
    vkDestroyFramebuffer(m_device, m_frame_buffer, nullptr);
    return true;
}
