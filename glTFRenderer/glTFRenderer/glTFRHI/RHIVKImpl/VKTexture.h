#pragma once
#include <vulkan/vulkan_core.h>

#include "glTFRHI/RHIInterface/IRHITexture.h"

class VKTexture : public IRHITexture
{
public:
    bool Init(VkDevice device, VkImage image, const RHITextureDesc& texture_desc);

    VkImage GetRawImage() const;
    
protected:
    VkDevice m_device {VK_NULL_HANDLE};
    VkImage m_image {VK_NULL_HANDLE};
};
