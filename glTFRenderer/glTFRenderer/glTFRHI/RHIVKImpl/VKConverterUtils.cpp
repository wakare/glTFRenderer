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

VkImageLayout VKConverterUtils::ConvertToImageLayout(RHIResourceStateType state)
{
    switch (state) {
    case RHIResourceStateType::STATE_UNKNOWN:
        return VK_IMAGE_LAYOUT_UNDEFINED;
        break;
    case RHIResourceStateType::STATE_COMMON:
        return VK_IMAGE_LAYOUT_GENERAL;
        break;
    case RHIResourceStateType::STATE_GENERIC_READ:
        return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
        break;
    case RHIResourceStateType::STATE_COPY_SOURCE:
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        break;
    case RHIResourceStateType::STATE_COPY_DEST:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        break;
    case RHIResourceStateType::STATE_VERTEX_AND_CONSTANT_BUFFER:
    GLTF_CHECK(false);
        break;
    case RHIResourceStateType::STATE_INDEX_BUFFER:
        GLTF_CHECK(false);
        break;
    case RHIResourceStateType::STATE_PRESENT:
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        break;
    case RHIResourceStateType::STATE_RENDER_TARGET:
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        break;
    case RHIResourceStateType::STATE_DEPTH_WRITE:
        return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        break;
    case RHIResourceStateType::STATE_DEPTH_READ:
        return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
        break;
    case RHIResourceStateType::STATE_UNORDERED_ACCESS:
        return VK_IMAGE_LAYOUT_GENERAL;
        break;
    case RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        break;
    case RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        break;
    case RHIResourceStateType::STATE_ALL_SHADER_RESOURCE:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        break;
    case RHIResourceStateType::STATE_RAYTRACING_ACCELERATION_STRUCTURE:
        GLTF_CHECK(false);
        break;
    }
    GLTF_CHECK(false);
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

VkFormat VKConverterUtils::ConvertToFormat(RHIDataFormat format)
{
    VkFormat result = VK_FORMAT_UNDEFINED;
    
    switch (format) {
    case RHIDataFormat::R8G8B8A8_UNORM:
        result = VK_FORMAT_R8G8B8A8_UNORM;
        break;
    case RHIDataFormat::R8G8B8A8_UNORM_SRGB:
        result = VK_FORMAT_R8G8B8A8_SRGB;
        break;
    case RHIDataFormat::B8G8R8A8_UNORM:
        result = VK_FORMAT_R8G8B8A8_UNORM;
        break;
    case RHIDataFormat::B8G8R8A8_UNORM_SRGB:
        result = VK_FORMAT_B8G8R8A8_SRGB;
        break;
    case RHIDataFormat::B8G8R8X8_UNORM:
        // TODO: No support format in vk
        result = VK_FORMAT_B8G8R8A8_UNORM;
        break;
    case RHIDataFormat::R32G32_FLOAT:
        result = VK_FORMAT_R32G32_SFLOAT;
        break;
    case RHIDataFormat::R32G32B32_FLOAT:
        result = VK_FORMAT_R32G32B32_SFLOAT;
        break;
    case RHIDataFormat::R32G32B32A32_FLOAT:
        result = VK_FORMAT_R32G32B32A32_SFLOAT;
        break;
    case RHIDataFormat::R16G16B16A16_FLOAT:
        result = VK_FORMAT_R16G16B16A16_SFLOAT;
        break;
    case RHIDataFormat::R16G16B16A16_UINT:
        result = VK_FORMAT_R16G16B16A16_UINT;
        break;
    case RHIDataFormat::R16G16B16A16_UNORM:
        result = VK_FORMAT_R16G16B16A16_UNORM;
        break;
    case RHIDataFormat::R10G10B10A2_UNORM:
        result = VK_FORMAT_A2R10G10B10_UNORM_PACK32;
        break;
    case RHIDataFormat::R10G10B10_XR_BIAS_A2_UNORM:
        // TODO: No support format in vk
        result = VK_FORMAT_A2R10G10B10_UNORM_PACK32;
        break;
    case RHIDataFormat::B5G5R5A1_UNORM:
        result = VK_FORMAT_B5G5R5A1_UNORM_PACK16;
        break;
    case RHIDataFormat::B5G6R5_UNORM:
        result = VK_FORMAT_B5G6R5_UNORM_PACK16;
        break;
    case RHIDataFormat::D32_FLOAT:
        result = VK_FORMAT_D32_SFLOAT;
        break;
    case RHIDataFormat::R32_FLOAT:
        result = VK_FORMAT_R32_SFLOAT;
        break;
    case RHIDataFormat::R32G32B32A32_UINT:
        result = VK_FORMAT_R32G32B32A32_UINT;
        break;
    case RHIDataFormat::R32_UINT:
        result = VK_FORMAT_R32_UINT;
        break;
    case RHIDataFormat::R32_TYPELESS:
        // TODO: No support format in vk
        result = VK_FORMAT_D32_SFLOAT;
        break;
    case RHIDataFormat::R16_FLOAT:
        result = VK_FORMAT_R16_SFLOAT;
        break;
    case RHIDataFormat::R16_UNORM:
        result = VK_FORMAT_R16_UNORM;
        break;
    case RHIDataFormat::R16_UINT:
        result = VK_FORMAT_R16_UINT;
        break;
    case RHIDataFormat::R8_UNORM:
        result = VK_FORMAT_R8_UNORM;
        break;
    case RHIDataFormat::A8_UNORM:
        result = VK_FORMAT_A8_UNORM_KHR;
        break;
    case RHIDataFormat::D32_SAMPLE_RESERVED:
        result = VK_FORMAT_D32_SFLOAT;
        break;
    case RHIDataFormat::UNKNOWN:
        result = VK_FORMAT_UNDEFINED;
        break;
    }

    //GLTF_CHECK(result != VK_FORMAT_UNDEFINED);
    return result;
}

VkSampleCountFlagBits VKConverterUtils::ConvertToSampleCount(RHISampleCount sample_count)
{
    VkSampleCountFlagBits result = VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;
    
    switch (sample_count)
    {
    case RHISampleCount::SAMPLE_1_BIT:
        result = VK_SAMPLE_COUNT_1_BIT;
        break;
    case RHISampleCount::SAMPLE_2_BIT:
        result = VK_SAMPLE_COUNT_2_BIT;
        break;
    case RHISampleCount::SAMPLE_4_BIT:
        result = VK_SAMPLE_COUNT_4_BIT;
        break;
    case RHISampleCount::SAMPLE_8_BIT:
        result = VK_SAMPLE_COUNT_8_BIT;
        break;
    case RHISampleCount::SAMPLE_16_BIT:
        result = VK_SAMPLE_COUNT_16_BIT;
        break;
    case RHISampleCount::SAMPLE_32_BIT:
        result = VK_SAMPLE_COUNT_32_BIT;
        break;
    case RHISampleCount::SAMPLE_64_BIT:
        result = VK_SAMPLE_COUNT_64_BIT;
        break;
    }

    return result;
}

VkAttachmentLoadOp VKConverterUtils::ConvertToLoadOp(RHIAttachmentLoadOp load_op)
{
    VkAttachmentLoadOp result = VK_ATTACHMENT_LOAD_OP_MAX_ENUM;
    
    switch (load_op) {
    case RHIAttachmentLoadOp::LOAD_OP_LOAD:
        result = VK_ATTACHMENT_LOAD_OP_LOAD;
        break;
    case RHIAttachmentLoadOp::LOAD_OP_CLEAR:
        result = VK_ATTACHMENT_LOAD_OP_CLEAR;
        break;
    case RHIAttachmentLoadOp::LOAD_OP_DONT_CARE:
        result = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        break;
    case RHIAttachmentLoadOp::LOAD_OP_NONE:
        result = VK_ATTACHMENT_LOAD_OP_NONE_EXT;
        break;
    }

    return result;
}

VkAttachmentStoreOp VKConverterUtils::ConvertToStoreOp(RHIAttachmentStoreOp store_op)
{
    VkAttachmentStoreOp result = VK_ATTACHMENT_STORE_OP_MAX_ENUM;
    
    switch (store_op) {
    case RHIAttachmentStoreOp::STORE_OP_STORE:
        result = VK_ATTACHMENT_STORE_OP_STORE;
        break;
    case RHIAttachmentStoreOp::STORE_OP_DONT_CARE:
        result = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        break;
    case RHIAttachmentStoreOp::STORE_OP_NONE:
        result = VK_ATTACHMENT_STORE_OP_NONE;
        break;
    }
    
    return result;
}

VkPipelineStageFlagBits VKConverterUtils::ConvertToPipelineStage(RHIPipelineStage stage)
{
    VkPipelineStageFlagBits result = VK_PIPELINE_STAGE_FLAG_BITS_MAX_ENUM;
    switch (stage) {
    case RHIPipelineStage::COLOR_ATTACHMENT_OUTPUT:
        result = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        break;
    }

    return result;
}

VkAccessFlagBits VKConverterUtils::ConvertToAccessFlags(RHIAccessFlags flags)
{
    VkAccessFlagBits result = VK_ACCESS_FLAG_BITS_MAX_ENUM;

    switch (flags) {
    case RHIAccessFlags::NONE:
        result = VK_ACCESS_NONE;
        break;
    case RHIAccessFlags::COLOR_ATTACHMENT_WRITE:
        result = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;
    }
    
    return result;
}

VkBufferUsageFlags VKConverterUtils::ConvertToBufferUsage(RHIResourceUsageFlags flags)
{
    VkBufferUsageFlags result{};

    if (flags & RUF_ALLOW_CBV)
    {
        result |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    
    if (flags & RUF_ALLOW_SRV)
    {
        result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }
    
    if (flags & RUF_ALLOW_UAV)
    {
        result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;    
    }
    
    if (flags & RUF_VERTEX_BUFFER)
    {
        result |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }

    if (flags & RUF_INDEX_BUFFER)
    {
        result |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }

    if (flags & RUF_INDIRECT_BUFFER)
    {
        result |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    }

    if (flags & RUF_TRANSFER_SRC)
    {
        result |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;    
    }
    
    if (flags & RUF_TRANSFER_DST)
    {
        result |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;    
    }
    
    return result;
}

VkImageUsageFlags VKConverterUtils::ConvertToImageUsage(RHIResourceUsageFlags flags)
{
    VkImageUsageFlags result{};
    
    if (flags & RUF_ALLOW_UAV)
    {
        result |= VK_IMAGE_USAGE_STORAGE_BIT;    
    }
    
    if (flags & RUF_ALLOW_SRV)
    {
        result |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }

    if (flags & RUF_ALLOW_DEPTH_STENCIL)
    {
        result |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }

    if (flags & RUF_ALLOW_RENDER_TARGET)
    {
        result |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }

    if (flags & RUF_TRANSFER_SRC)
    {
        result |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;    
    }
    
    if (flags & RUF_TRANSFER_DST)
    {
        result |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;    
    }
    
    return result;
}

VkPrimitiveTopology VKConverterUtils::ConvertToPrimitiveTopology(RHIPrimitiveTopologyType type)
{
    VkPrimitiveTopology result = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
    switch (type) {
    case RHIPrimitiveTopologyType::TRIANGLELIST:
        result = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        break;
    default:
        GLTF_CHECK(false);
    }
    
    return result;
}
