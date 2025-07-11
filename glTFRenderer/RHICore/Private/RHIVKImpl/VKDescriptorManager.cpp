#include "VKDescriptorManager.h"

#include "VKBuffer.h"
#include "VKConverterUtils.h"
#include "VKDevice.h"
#include "VKTexture.h"
#include "VKCommon.h"
#include "RHIResourceFactoryImpl.hpp"
#include "RHIInterface/IRHIMemoryManager.h"

bool VKBufferDescriptorAllocation::InitFromBuffer(const std::shared_ptr<IRHIBuffer>& buffer, const RHIBufferDescriptorDesc& desc)
{
    m_view_desc = desc;
    m_source = buffer;
    m_buffer_init = true;
    need_release = false;
    
    return true;
}

VkBuffer VKBufferDescriptorAllocation::GetRawBuffer() const
{
    GLTF_CHECK(m_buffer_init);
    return dynamic_cast<const VKBuffer&>(*m_source).GetRawBuffer();
}

bool VKTextureDescriptorAllocation::InitFromImageView(const std::shared_ptr<IRHITexture>& texture, VkDevice device, VkImageView image_view, const RHITextureDescriptorDesc& desc)
{
    m_device = device;
    m_view_desc = desc;
    m_source = texture;
    m_image_view = image_view;
    m_image_init = true;
    need_release = true;
    
    return true;
}

VkImageView VKTextureDescriptorAllocation::GetRawImageView() const
{
    return m_image_view;
}

bool VKTextureDescriptorAllocation::Release(IRHIMemoryManager& memory_manager)
{
    if (!need_release)
    {
        return true;
    }

    need_release = false;
    vkDestroyImageView(m_device, m_image_view, nullptr);
    
    return true;
}

bool VKDescriptorTable::Build(IRHIDevice& device,
                              const std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>>& descriptor_allocations)
{
    GLTF_CHECK(!descriptor_allocations.empty());
    
    for (const auto& descriptor : descriptor_allocations)
    {
        VkImageView view = dynamic_cast<const VKTextureDescriptorAllocation&>(*descriptor).GetRawImageView();
        VkDescriptorImageInfo image_info{};
        image_info.imageView = view;
        switch (descriptor->GetDesc().m_view_type) {
        case RHIViewType::RVT_SRV:
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            break;
        case RHIViewType::RVT_UAV:
            image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            break;
        default:
            GLTF_CHECK(false);
        }
        m_image_infos.push_back(image_info);
    }
    
    return true;
}

const std::vector<VkDescriptorImageInfo>& VKDescriptorTable::GetImageInfos() const
{
    return m_image_infos;
}

bool VKDescriptorManager::Init(IRHIDevice& device, const DescriptorAllocationInfo& max_descriptor_capacity)
{
    m_device = dynamic_cast<const VKDevice&>(device).GetDevice();

    // TODO: re-design descriptor capacity
    std::vector<VkDescriptorPoolSize> pool_sizes;
    pool_sizes.push_back({VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, max_descriptor_capacity.cbv_srv_uav_size});
    pool_sizes.push_back({VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, max_descriptor_capacity.cbv_srv_uav_size});
    pool_sizes.push_back({VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, max_descriptor_capacity.cbv_srv_uav_size});
    pool_sizes.push_back({VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, max_descriptor_capacity.cbv_srv_uav_size});
    
    VkDescriptorPoolCreateInfo descriptor_pool_create_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .pNext = nullptr};
    descriptor_pool_create_info.poolSizeCount = pool_sizes.size();
    descriptor_pool_create_info.pPoolSizes = pool_sizes.data();
    descriptor_pool_create_info.maxSets = 64;
    descriptor_pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    
    VK_CHECK(vkCreateDescriptorPool(m_device, &descriptor_pool_create_info, nullptr, &m_descriptor_pool));
    need_release = true;
    
    return true;
}

bool VKDescriptorManager::CreateDescriptor(IRHIDevice& device, const std::shared_ptr<IRHIBuffer>& buffer, const RHIBufferDescriptorDesc& desc,
                                           std::shared_ptr<IRHIBufferDescriptorAllocation>& out_descriptor_allocation)
{
    std::shared_ptr<IRHIBufferDescriptorAllocation> allocation = RHIResourceFactory::CreateRHIResource<IRHIBufferDescriptorAllocation>();
    allocation->InitFromBuffer(buffer, desc);
    out_descriptor_allocation = allocation;
    return true;
}

bool VKDescriptorManager::CreateDescriptor(IRHIDevice& device, const std::shared_ptr<IRHITexture>& texture, const RHITextureDescriptorDesc& desc,
                                           std::shared_ptr<IRHITextureDescriptorAllocation>& out_descriptor_allocation)
{
    auto vk_device = dynamic_cast<VKDevice&>(device).GetDevice();
    auto vk_image = dynamic_cast<const VKTexture&>(*texture).GetRawImage();
    
    VkImageViewCreateInfo image_view_create_info{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .pNext = nullptr};
    image_view_create_info.image = vk_image;
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.format = VKConverterUtils::ConvertToFormat(desc.m_format);
    image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    
    image_view_create_info.subresourceRange.aspectMask = desc.m_view_type == RHIViewType::RVT_DSV ? VK_IMAGE_ASPECT_DEPTH_BIT :
        desc.m_format == RHIDataFormat::D32_SAMPLE_RESERVED ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    
    image_view_create_info.subresourceRange.baseMipLevel = 0;
    image_view_create_info.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS ;
    image_view_create_info.subresourceRange.baseArrayLayer = 0;
    image_view_create_info.subresourceRange.layerCount = 1;

    VkImageView out_image_view = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(vk_device, &image_view_create_info, nullptr, &out_image_view));
    std::shared_ptr<IRHITextureDescriptorAllocation> allocation = RHIResourceFactory::CreateRHIResource<IRHITextureDescriptorAllocation>();
    dynamic_cast<VKTextureDescriptorAllocation&>(*allocation).InitFromImageView(texture, vk_device, out_image_view, desc);
    out_descriptor_allocation = allocation;
    
    return true;
}

bool VKDescriptorManager::BindDescriptorContext(IRHICommandList& command_list)
{
    return true;
}

bool VKDescriptorManager::BindGUIDescriptorContext(IRHICommandList& command_list)
{
    return true;
}

bool VKDescriptorManager::Release(IRHIMemoryManager& memory_manager)
{
    if (!need_release)
    {
        return true;
    }

    need_release = false;
    vkDestroyDescriptorPool(m_device, m_descriptor_pool, nullptr);

    return true;
}

VkDescriptorPool VKDescriptorManager::GetDescriptorPool() const
{
    return m_descriptor_pool;
}
