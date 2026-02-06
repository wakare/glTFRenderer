#include "VKDescriptorUpdater.h"

#include "VKDescriptorManager.h"
#include "VKDevice.h"
#include "VKRootSignature.h"

bool VKDescriptorUpdater::BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline,
                                         const RootSignatureAllocation& root_signature_allocation, const IRHIDescriptorAllocation& allocation)
{
    if (root_signature_allocation.type == RHIRootParameterType::AccelerationStructure)
    {
        const auto& accel_allocation = dynamic_cast<const VKAccelerationStructureDescriptorAllocation&>(allocation);
        if (accel_allocation.GetAccelerationStructure() == VK_NULL_HANDLE)
        {
            return true;
        }

        auto& accel_handle = m_cache_accel_handles.emplace_back(accel_allocation.GetAccelerationStructure());
        auto& accel_info = m_cache_accel_infos.emplace_back();
        accel_info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        accel_info.pNext = nullptr;
        accel_info.accelerationStructureCount = 1;
        accel_info.pAccelerationStructures = &accel_handle;

        VkWriteDescriptorSet write_set{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = &accel_info};
        write_set.dstBinding = root_signature_allocation.local_space_parameter_index;
        write_set.descriptorCount = 1;
        write_set.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        write_set.dstSet = VK_NULL_HANDLE;

        m_cache_descriptor_writers[root_signature_allocation.space].push_back(write_set);
        return true;
    }

    const auto& descriptor_desc = allocation.GetDesc();

    VkWriteDescriptorSet draw_image_write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = nullptr};
    draw_image_write.dstBinding = root_signature_allocation.local_space_parameter_index;
    draw_image_write.descriptorCount = 1;

    // bind descriptor set in finalize phase
    draw_image_write.dstSet = VK_NULL_HANDLE;

    if (descriptor_desc.IsBufferDescriptor())
    {
        const auto& buffer_desc = dynamic_cast<const RHIBufferDescriptorDesc&>(descriptor_desc);
        // Buffer resource
        auto& buffer_info = m_cache_buffer_infos.emplace_back();
        buffer_info.buffer = dynamic_cast<const VKBufferDescriptorAllocation&>(allocation).GetRawBuffer();
        buffer_info.range = VK_WHOLE_SIZE;
        buffer_info.offset = buffer_desc.m_offset;
        
        switch (descriptor_desc.m_view_type)
        {
        case RHIViewType::RVT_CBV:
            draw_image_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            break;
        case RHIViewType::RVT_SRV:
            draw_image_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            break;
        case RHIViewType::RVT_UAV:
            draw_image_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            break;
        }
        draw_image_write.pBufferInfo = &buffer_info;
    }
    else if (descriptor_desc.IsTextureDescriptor())
    {
        // Texture resource
        auto& VkTextureDesc = dynamic_cast<const VKTextureDescriptorAllocation&>(allocation);
        
        auto& image_info = m_cache_image_infos.emplace_back();
        image_info.imageView = VkTextureDesc.GetRawImageView();
        // No sampler because do not support combined sampler descriptor type
        image_info.sampler = VK_NULL_HANDLE;
        
        switch (descriptor_desc.m_view_type) {
        case RHIViewType::RVT_DSV:
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
            // cpu descriptor should bind with other API
            GLTF_CHECK(false);
            break;
        }
        draw_image_write.pImageInfo = &image_info;
    }
    
    m_cache_descriptor_writers[root_signature_allocation.space].push_back(draw_image_write);
    
    return true;
}

bool VKDescriptorUpdater::BindDescriptor(IRHICommandList& command_list, RHIPipelineType pipeline, const RootSignatureAllocation& root_signature_allocation,
                                                     const IRHIDescriptorTable& allocation_table, RHIDescriptorRangeType descriptor_type)
{
    const auto& image_infos = dynamic_cast<const VKDescriptorTable&>(allocation_table).GetImageInfos();

    VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    switch (descriptor_type)
    {
    case RHIDescriptorRangeType::SRV:
        type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        break;
    case RHIDescriptorRangeType::UAV:
        type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        break;
    }
    
    for (size_t i = 0; i < image_infos.size(); ++i)
    {
        VkWriteDescriptorSet draw_image_write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = nullptr};
        draw_image_write.dstBinding = root_signature_allocation.local_space_parameter_index;
        draw_image_write.descriptorCount = 1;
        draw_image_write.dstSet = VK_NULL_HANDLE;
        draw_image_write.dstArrayElement = i;
        draw_image_write.descriptorType = type;
        draw_image_write.pImageInfo = &image_infos[i];
        m_cache_descriptor_writers[root_signature_allocation.space].push_back(draw_image_write);
    }
    
    return true;
}

bool VKDescriptorUpdater::FinalizeUpdateDescriptors(IRHIDevice& device, IRHICommandList& command_list, IRHIRootSignature& root_signature)
{
    if (m_cache_descriptor_writers.empty())
    {
        return true;
    }

    // Can not write to static sampler descriptor layout
    const auto& vk_descriptor_sets = dynamic_cast<VKRootSignature&>(root_signature).GetDescriptorSets();
    auto vk_device = dynamic_cast<VKDevice&>(device).GetDevice();
    if (!vk_descriptor_sets.empty())
    {
        for (auto& writer_info : m_cache_descriptor_writers)
        {
            const auto& write_set = vk_descriptor_sets[writer_info.first];
            for (auto& writer : writer_info.second )
            {
                writer.dstSet = write_set;    
            }

            vkUpdateDescriptorSets(vk_device, writer_info.second.size(), writer_info.second.data(), 0, nullptr);
        }    
    }

    m_cache_descriptor_writers.clear();
    m_cache_buffer_infos.clear();
    m_cache_image_infos.clear();
    m_cache_accel_handles.clear();
    m_cache_accel_infos.clear();
    
    return true;
}
