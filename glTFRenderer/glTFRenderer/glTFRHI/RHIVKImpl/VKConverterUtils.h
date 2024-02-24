#pragma once
#include <vulkan/vulkan_core.h>

#include "glTFRHI/RHIInterface/IRHISubPass.h"

class VKConverterUtils
{
public:
    static VkPipelineBindPoint ConvertToBindPoint(RHISubPassBindPoint bind_point);
    static VkImageLayout ConvertToImageLayout(RHIImageLayout image_layout);
};
