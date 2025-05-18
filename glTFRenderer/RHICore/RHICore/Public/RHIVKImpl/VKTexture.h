#pragma once
#include "VolkUtils.h"
#include "IRHITexture.h"

class VKTexture : public IRHITexture
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKTexture)
    
    bool Init(VkDevice device, VkImage image, const RHITextureDesc& texture_desc);

    VkImage GetRawImage() const;
    
protected:
    VkDevice m_device {VK_NULL_HANDLE};
    VkImage m_image {VK_NULL_HANDLE};
};
