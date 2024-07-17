#include "VKTexture.h"

#include "VKBuffer.h"

bool VKTexture::Init(VkDevice device, VkImage image, const RHITextureDesc& texture_desc)
{
    m_texture_desc = texture_desc;
    m_device = device;
    m_image = image;
    
    return true;
}

VkImage VKTexture::GetRawImage() const
{
    return m_image;
}
