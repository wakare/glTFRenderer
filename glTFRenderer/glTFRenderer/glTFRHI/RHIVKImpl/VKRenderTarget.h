#pragma once
#include <vulkan/vulkan_core.h>

#include "glTFRHI/RHIInterface/IRHIRenderTarget.h"

class VKRenderTarget : public IRHIRenderTarget
{
public:
    VKRenderTarget();
    virtual ~VKRenderTarget() override;
    
    bool InitRenderTarget(VkDevice device, VkFormat format, VkImage image);
    
protected:
    VkDevice device;
    VkImageView image_view;
};
