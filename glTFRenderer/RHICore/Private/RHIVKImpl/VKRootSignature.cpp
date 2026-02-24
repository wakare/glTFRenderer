#include "VKRootSignature.h"

#include "VKDescriptorManager.h"
#include "VKDevice.h"
#include "VKRootParameter.h"
#include "VKCommon.h"
#include "VKStaticSampler.h"
#include "RHIInterface/IRHIMemoryManager.h"

bool VKRootSignature::InitRootSignature(IRHIDevice& device, IRHIDescriptorManager& descriptor_manager)
{
    if (m_root_parameters.empty())
    {
        return true;
    }
    
    m_device = dynamic_cast<VKDevice&>(device).GetDevice();
    auto vk_descriptor_pool = dynamic_cast<VKDescriptorManager&>(descriptor_manager).GetDescriptorPool();

    int max_set_index = -1;
    std::map<unsigned, std::vector<VkDescriptorSetLayoutBinding>> space_bindings;
    std::map<unsigned, std::vector<VkDescriptorBindingFlags>> space_bind_flags;
    for (const auto& parameter : m_root_parameters)
    {
        VKRootParameter& vk_root_parameter = dynamic_cast<VKRootParameter&>(*parameter);
        space_bindings[vk_root_parameter.GetRegisterSpace()].push_back(vk_root_parameter.GetRawLayoutBinding());

        //VkDescriptorBindingFlags flag = vk_root_parameter.IsBindless() ? VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT : 0;
        VkDescriptorBindingFlags flag = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
        space_bind_flags[vk_root_parameter.GetRegisterSpace()].push_back(flag);

        max_set_index = std::max(static_cast<int>(vk_root_parameter.GetRegisterSpace()), max_set_index);
    }
    
    for (const auto& parameter : m_static_samplers)
    {
        VKStaticSampler& vk_static_sampler = dynamic_cast<VKStaticSampler&>(*parameter);
        space_bindings[vk_static_sampler.GetRegisterSpace()].push_back(vk_static_sampler.GetRawLayoutBinding());
        space_bind_flags[vk_static_sampler.GetRegisterSpace()].push_back({0});

        max_set_index = std::max(static_cast<int>(vk_static_sampler.GetRegisterSpace()), max_set_index);
    }

    m_descriptor_set_layouts.resize(max_set_index + 1);
    for (size_t i = 0; i < m_descriptor_set_layouts.size(); ++i)
    {
        const auto& binding = space_bindings[i];
        const auto& flags = space_bind_flags[i];

        VkDescriptorSetLayoutBindingFlagsCreateInfo flags_create_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
        flags_create_info.pNext = nullptr;
        flags_create_info.pBindingFlags = flags.data();
        flags_create_info.bindingCount = flags.size();
        
        VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .pNext = &flags_create_info};
        descriptor_set_layout_create_info.pBindings = binding.data();
        descriptor_set_layout_create_info.bindingCount = binding.size();
        descriptor_set_layout_create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        
        VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptor_set_layout_create_info, nullptr, &m_descriptor_set_layouts[i]));
    }
    
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .pNext = nullptr};
    descriptor_set_allocate_info.descriptorPool = vk_descriptor_pool;
    descriptor_set_allocate_info.descriptorSetCount = static_cast<uint32_t>(m_descriptor_set_layouts.size());
    descriptor_set_allocate_info.pSetLayouts = m_descriptor_set_layouts.data();

    // Descriptor set allocation count must match set layout count (max space index + 1).
    // Using space_bindings.size() can under-allocate when spaces are sparse (e.g. only set 2),
    // which causes vkAllocateDescriptorSets to write out of bounds.
    m_descriptor_sets.resize(m_descriptor_set_layouts.size());
    VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptor_set_allocate_info, m_descriptor_sets.data()));
    need_release = true;
    
    return true;
}

bool VKRootSignature::Release(IRHIMemoryManager& memory_manager)
{
    if (!need_release)
    {
        return true;
    }

    need_release = false;

    if (!m_descriptor_sets.empty())
    {
        auto& vk_descriptor_manager = dynamic_cast<VKDescriptorManager&>(memory_manager.GetDescriptorManager());
        const auto descriptor_pool = vk_descriptor_manager.GetDescriptorPool();
        if (descriptor_pool != VK_NULL_HANDLE)
        {
            VK_CHECK(vkFreeDescriptorSets(
                m_device,
                descriptor_pool,
                static_cast<uint32_t>(m_descriptor_sets.size()),
                m_descriptor_sets.data()));
        }
        m_descriptor_sets.clear();
    }

    for (const auto& descriptor_set_layout : m_descriptor_set_layouts)
    {
        vkDestroyDescriptorSetLayout(m_device, descriptor_set_layout, nullptr);    
    }
    m_descriptor_set_layouts.clear();
    
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
