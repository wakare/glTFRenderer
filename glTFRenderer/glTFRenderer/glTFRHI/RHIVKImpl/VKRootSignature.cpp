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

    std::map<unsigned, std::vector<VkDescriptorSetLayoutBinding>> space_bindings;
    for (const auto& parameter : m_root_parameters)
    {
        VKRootParameter& vk_root_parameter = dynamic_cast<VKRootParameter&>(*parameter);
        space_bindings[vk_root_parameter.GetRegisterSpace()].push_back(vk_root_parameter.GetRawLayoutBinding());
    }
    
    for (const auto& parameter : m_static_samplers)
    {
        VKStaticSampler& vk_static_sampler = dynamic_cast<VKStaticSampler&>(*parameter);
        space_bindings[vk_static_sampler.GetRegisterSpace()].push_back(vk_static_sampler.GetRawLayoutBinding());
    }

    m_descriptor_set_layouts.resize(space_bindings.size());
    for (size_t i = 0; i < m_descriptor_set_layouts.size(); ++i)
    {
        const auto& binding = space_bindings[i];
        
        VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .pNext = nullptr};
        descriptor_set_layout_create_info.pBindings = binding.data();
        descriptor_set_layout_create_info.bindingCount = binding.size();

        VK_CHECK(vkCreateDescriptorSetLayout(vk_device, &descriptor_set_layout_create_info, nullptr, &m_descriptor_set_layouts[i]));
    }
    
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .pNext = nullptr};
    descriptor_set_allocate_info.descriptorPool = vk_descriptor_pool;
    descriptor_set_allocate_info.descriptorSetCount = m_descriptor_set_layouts.size();
    descriptor_set_allocate_info.pSetLayouts = m_descriptor_set_layouts.data();

    m_descriptor_sets.resize(space_bindings.size());
    VK_CHECK(vkAllocateDescriptorSets(vk_device, &descriptor_set_allocate_info, m_descriptor_sets.data()));
    
    return true;
}

const std::vector<VkDescriptorSet>& VKRootSignature::GetDescriptorSets() const
{
    return m_descriptor_sets;
}

const std::vector<VkDescriptorSetLayout>& VKRootSignature::GetDescriptorSetLayouts() const
{
    return m_descriptor_set_layouts;
}
