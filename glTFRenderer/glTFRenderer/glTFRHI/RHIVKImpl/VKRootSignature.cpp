#include "VKRootSignature.h"

#include "VKDescriptorManager.h"
#include "VKDevice.h"
#include "VKRootParameter.h"
#include "VKCommon.h"
#include "VKStaticSampler.h"

bool VKRootSignature::InitRootSignature(IRHIDevice& device, IRHIDescriptorManager& descriptor_manager)
{
    auto vk_device = dynamic_cast<VKDevice&>(device).GetDevice();
    auto vk_descriptor_pool = dynamic_cast<VKDescriptorManager&>(descriptor_manager).GetDesciptorPool();

    // Create descriptor set layout for resource
    {
        std::vector<VkDescriptorSetLayoutBinding> descriptor_set_layout_bindings;
        descriptor_set_layout_bindings.reserve(m_root_parameters.size());
        for (const auto& parameter : m_root_parameters)
        {
            descriptor_set_layout_bindings.push_back(dynamic_cast<const VKRootParameter&>(*parameter).GetRawLayoutBinding());
        }

        VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .pNext = nullptr};
        descriptor_set_layout_create_info.pBindings = descriptor_set_layout_bindings.data();
        descriptor_set_layout_create_info.bindingCount = descriptor_set_layout_bindings.size();

        VK_CHECK(vkCreateDescriptorSetLayout(vk_device, &descriptor_set_layout_create_info, nullptr, &m_descriptor_set_layout_resource));

        VkDescriptorSetAllocateInfo descriptor_set_allocate_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .pNext = nullptr};
        descriptor_set_allocate_info.descriptorPool = vk_descriptor_pool;
        descriptor_set_allocate_info.descriptorSetCount = 1;
        descriptor_set_allocate_info.pSetLayouts = &m_descriptor_set_layout_resource;
    
        VK_CHECK(vkAllocateDescriptorSets(vk_device, &descriptor_set_allocate_info, &m_descriptor_set_resource));
    }
    
    
    // Create descriptor set layout for sampler
    if (HasSampler())
    {
        std::vector<VkDescriptorSetLayoutBinding> descriptor_set_layout_bindings;
        descriptor_set_layout_bindings.reserve(m_static_samplers.size());
        for (const auto& parameter : m_static_samplers)
        {
            descriptor_set_layout_bindings.push_back(dynamic_cast<const VKStaticSampler&>(*parameter).GetRawLayoutBinding());
        }

        VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .pNext = nullptr};
        descriptor_set_layout_create_info.pBindings = descriptor_set_layout_bindings.data();
        descriptor_set_layout_create_info.bindingCount = descriptor_set_layout_bindings.size();

        VK_CHECK(vkCreateDescriptorSetLayout(vk_device, &descriptor_set_layout_create_info, nullptr, &m_descriptor_set_layout_sampler));
        
        VkDescriptorSetAllocateInfo descriptor_set_allocate_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .pNext = nullptr};
        descriptor_set_allocate_info.descriptorPool = vk_descriptor_pool;
        descriptor_set_allocate_info.descriptorSetCount = 1;
        descriptor_set_allocate_info.pSetLayouts = &m_descriptor_set_layout_sampler;
        
        VK_CHECK(vkAllocateDescriptorSets(vk_device, &descriptor_set_allocate_info, &m_descriptor_set_sampler));
    }

    return true;
}

VkDescriptorSetLayout VKRootSignature::GetDescriptorSetLayoutResource() const
{
    return m_descriptor_set_layout_resource;
}

VkDescriptorSetLayout VKRootSignature::GetDescriptorSetLayoutSampler() const
{
    GLTF_CHECK(HasSampler());
    return m_descriptor_set_layout_sampler;
}

VkDescriptorSet VKRootSignature::GetDescriptorSetResource() const
{
    return m_descriptor_set_resource;
}

VkDescriptorSet VKRootSignature::GetDescriptorSetSampler() const
{
    GLTF_CHECK(HasSampler());
    return m_descriptor_set_sampler;
}
