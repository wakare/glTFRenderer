#include "VKDescriptorUpdater.h"

#include "VKDescriptorManager.h"
#include "VKDevice.h"
#include "VKRootSignature.h"

bool VKDescriptorUpdater::BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline,
                                         unsigned slot, const IRHIDescriptorAllocation& allocation)
{
    const auto& view_info = allocation.GetDesc();
    
    VkWriteDescriptorSet draw_image_write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = nullptr};
    draw_image_write.dstBinding = slot;
    draw_image_write.descriptorCount = 1;
    
    // bind descriptor set in finalize phase
    draw_image_write.dstSet = VK_NULL_HANDLE;
    
    if (view_info.IsBufferDescriptor())
    {
        const auto& buffer_desc = dynamic_cast<const RHIBufferDescriptorDesc&>(view_info);
        // Buffer resource
        auto& buffer_info = m_cache_buffer_infos.emplace_back();
        buffer_info.buffer = dynamic_cast<const VKBufferDescriptorAllocation&>(allocation).GetRawBuffer();
        buffer_info.range = buffer_desc.m_size;
        buffer_info.offset = buffer_desc.m_offset;
    }
    else if (view_info.IsTextureDescriptor())
    {
        // Texture resource
        auto& VkTextureDesc = dynamic_cast<const VKTextureDescriptorAllocation&>(allocation);
        
        auto& image_info = m_cache_image_infos.emplace_back();
        image_info.imageView = VkTextureDesc.GetRawImageView();
        // No sampler because do not support combined sampler descriptor type
        image_info.sampler = VK_NULL_HANDLE;
        
        switch (view_info.m_view_type) {
        case RHIViewType::RVT_SRV:
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;    
            draw_image_write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            break;
        case RHIViewType::RVT_UAV:
            image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            draw_image_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            break;
        case RHIViewType::RVT_CBV:
        case RHIViewType::RVT_RTV:
        case RHIViewType::RVT_DSV:
            // cpu descriptor should bind with other API
            GLTF_CHECK(false);
            break;
        }
        draw_image_write.pImageInfo = &image_info;
    }
    
    m_cache_descriptor_writers.push_back(draw_image_write);
    
    return true;
}

bool VKDescriptorUpdater::BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline, unsigned slot,
    const IRHIDescriptorTable& allocation_table)
{
    return false;
}

bool VKDescriptorUpdater::FinalizeUpdateDescriptors(IRHIDevice& device, IRHICommandList& command_list, IRHIRootSignature& root_signature)
{
    GLTF_CHECK(!m_cache_descriptor_writers.empty());
    
    auto vk_Descriptor_set = dynamic_cast<VKRootSignature&>(root_signature).GetDescriptorSet();
    auto vk_device = dynamic_cast<VKDevice&>(device).GetDevice();
    
    for (auto& writer : m_cache_descriptor_writers)
    {
        writer.dstSet = vk_Descriptor_set;
    }

    vkUpdateDescriptorSets(vk_device, m_cache_descriptor_writers.size(), m_cache_descriptor_writers.data(), 0, nullptr);
    m_cache_descriptor_writers.clear();
    
    return true;
}
