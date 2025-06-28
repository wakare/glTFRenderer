#pragma once
#include "VolkUtils.h"
#include "RHIInterface/IRHISubPass.h"

class VKConverterUtils
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKConverterUtils)
    
    static VkPipelineBindPoint ConvertToBindPoint(RHISubPassBindPoint bind_point);
    static VkImageLayout ConvertToImageLayout(RHIImageLayout image_layout);
    static VkImageLayout ConvertToImageLayout(RHIResourceStateType state);
    static VkFormat ConvertToFormat(RHIDataFormat format);
    static VkSampleCountFlagBits ConvertToSampleCount(RHISampleCount sample_count);
    static VkAttachmentLoadOp ConvertToLoadOp(RHIAttachmentLoadOp load_op);
    static VkAttachmentStoreOp ConvertToStoreOp(RHIAttachmentStoreOp store_op);
    static VkPipelineStageFlagBits ConvertToPipelineStage(RHIPipelineStage stage);
    static VkAccessFlagBits ConvertToAccessFlags(RHIAccessFlags flags);
    static VkBufferUsageFlags ConvertToBufferUsage(RHIResourceUsageFlags flags);
    static VkImageUsageFlags ConvertToImageUsage(RHIResourceUsageFlags flags);
    static VkPrimitiveTopology ConvertToPrimitiveTopology(RHIPrimitiveTopologyType type);
};
