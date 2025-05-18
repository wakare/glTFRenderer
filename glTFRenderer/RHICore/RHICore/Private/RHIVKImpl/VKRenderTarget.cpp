#include "VKRenderTarget.h"

VKRenderTarget::VKRenderTarget()
    : m_device(VK_NULL_HANDLE)
    , m_image_view(VK_NULL_HANDLE)
{
    
}

VKRenderTarget::~VKRenderTarget()
{
    vkDestroyImageView(m_device, m_image_view, nullptr);
}

bool VKRenderTarget::InitRenderTarget(VkDevice vk_device, VkFormat format, VkImage image)
{
    m_device = vk_device;
    
    VkImageViewCreateInfo create_image_view_info{};
    create_image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_image_view_info.image = image;
    create_image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    create_image_view_info.format = format;
    create_image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    create_image_view_info.subresourceRange.baseMipLevel = 0;
    create_image_view_info.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS ;
    create_image_view_info.subresourceRange.baseArrayLayer = 0;
    create_image_view_info.subresourceRange.layerCount = 1;

    VkResult result = vkCreateImageView(vk_device, &create_image_view_info, nullptr, &m_image_view);
    GLTF_CHECK(result == VK_SUCCESS);

    return true;
}

const VkImageView& VKRenderTarget::GetImageView() const
{
    return m_image_view;
}
