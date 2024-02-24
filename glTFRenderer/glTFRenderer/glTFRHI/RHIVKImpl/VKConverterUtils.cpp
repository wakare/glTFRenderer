#include "VKConverterUtils.h"

VkPipelineBindPoint VKConverterUtils::ConvertToBindPoint(RHISubPassBindPoint bind_point)
{
    VkPipelineBindPoint result = VK_PIPELINE_BIND_POINT_MAX_ENUM;
    switch (bind_point) {
    case RHISubPassBindPoint::GRAPHICS:
        result = VK_PIPELINE_BIND_POINT_GRAPHICS;
        break;
    case RHISubPassBindPoint::COMPUTE:
        result = VK_PIPELINE_BIND_POINT_COMPUTE;
        break;
    case RHISubPassBindPoint::RAYTRACING:
        result = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
        break;
    }

    return result;
}

VkImageLayout VKConverterUtils::ConvertToImageLayout(RHIImageLayout image_layout)
{
    VkImageLayout result = VK_IMAGE_LAYOUT_MAX_ENUM;
    
    switch (image_layout) {
    case RHIImageLayout::UNDEFINED:
        result = VK_IMAGE_LAYOUT_UNDEFINED;
        break;
    case RHIImageLayout::GENERAL:
        result = VK_IMAGE_LAYOUT_GENERAL;
        break;
    case RHIImageLayout::COLOR_ATTACHMENT:
        result = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        break;
    }

    return result;
}
