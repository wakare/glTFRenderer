#include "VKTexture.h"

#include "VKBuffer.h"

bool VKTexture::Init(VkDevice device, VkImage image)
{
    m_device = device;
    m_image = image;
    
    return true;
}
