#pragma once
#include "VolkUtils.h"
#include "RHIInterface/IRHIRenderTarget.h"

class VKRenderTarget : public IRHIRenderTarget
{
public:
    VKRenderTarget();
    virtual ~VKRenderTarget() override;
    
    bool InitRenderTarget(VkDevice device, VkFormat format, VkImage image);

    const VkImageView& GetImageView() const;
    
protected:
    VkDevice m_device;
    VkImageView m_image_view;
};
