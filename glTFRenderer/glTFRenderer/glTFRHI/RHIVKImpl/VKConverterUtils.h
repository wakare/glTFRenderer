#pragma once
#include <vulkan/vulkan_core.h>

#include "glTFRHI/RHIInterface/IRHISubPass.h"

class VKConverterUtils
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKConverterUtils)
    
    static VkPipelineBindPoint ConvertToBindPoint(RHISubPassBindPoint bind_point);
    static VkImageLayout ConvertToImageLayout(RHIImageLayout image_layout);
    static VkFormat ConvertToFormat(RHIDataFormat format);
    static VkSampleCountFlagBits ConvertToSampleCount(RHISampleCount sample_count);
    static VkAttachmentLoadOp ConvertToLoadOp(RHIAttachmentLoadOp load_op);
    static VkAttachmentStoreOp ConvertToStoreOp(RHIAttachmentStoreOp store_op);
    static VkPipelineStageFlagBits ConvertToPipelineStage(RHIPipelineStage stage);
    static VkAccessFlagBits ConvertToAccessFlags(RHIAccessFlags flags);
};
