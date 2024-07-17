#include "VKRootSignature.h"

#include "VKDescriptorManager.h"
#include "VKDevice.h"
#include "VKRootParameter.h"
#include "VKCommon.h"

bool VKRootSignature::InitRootSignature(IRHIDevice& device, IRHIDescriptorManager& descriptor_manager)
{
    auto vk_device = dynamic_cast<VKDevice&>(device).GetDevice();
    auto vk_descriptor_pool = dynamic_cast<VKDescriptorManager&>(descriptor_manager).GetDesciptorPool();
    
    // Create descriptor set layout
    std::vector<VkDescriptorSetLayoutBinding> descriptor_set_layout_bindings(m_root_parameters.size());
    for (const auto& parameter : m_root_parameters)
    {
        descriptor_set_layout_bindings.push_back(dynamic_cast<const VKRootParameter&>(*parameter).m_binding);
    }

    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .pNext = nullptr};
    descriptor_set_layout_create_info.pBindings = descriptor_set_layout_bindings.data();
    descriptor_set_layout_create_info.bindingCount = descriptor_set_layout_bindings.size();

    VK_CHECK(vkCreateDescriptorSetLayout(vk_device, &descriptor_set_layout_create_info, nullptr, &m_descriptor_set_layout));

    VkDescriptorSetAllocateInfo descriptor_set_allocate_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .pNext = nullptr};
    descriptor_set_allocate_info.descriptorPool = vk_descriptor_pool;
    descriptor_set_allocate_info.descriptorSetCount = 1;
    descriptor_set_allocate_info.pSetLayouts = &m_descriptor_set_layout;
    
    VK_CHECK(vkAllocateDescriptorSets(vk_device, &descriptor_set_allocate_info, &m_descriptor_set));
    
    return true;
}

VkDescriptorSetLayout VKRootSignature::GetDescriptorSetLayout() const
{
    return m_descriptor_set_layout;
}

VkDescriptorSet VKRootSignature::GetDescriptorSet() const
{
    return m_descriptor_set;
}
