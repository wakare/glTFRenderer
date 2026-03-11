#include "VKRootSignature.h"

#include "VKDescriptorManager.h"
#include "VKDevice.h"
#include "VKRootParameter.h"
#include "VKCommon.h"
#include "VKStaticSampler.h"
#include "RHIInterface/IRHICommandList.h"
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
    m_descriptor_set_frame_slot_count = descriptor_manager.GetFrameSlotCount();
    std::vector<VkDescriptorSetLayout> descriptor_set_layouts_for_frames;
    descriptor_set_layouts_for_frames.reserve(m_descriptor_set_layouts.size() * m_descriptor_set_frame_slot_count);
    for (unsigned frame_slot = 0; frame_slot < m_descriptor_set_frame_slot_count; ++frame_slot)
    {
        descriptor_set_layouts_for_frames.insert(
            descriptor_set_layouts_for_frames.end(),
            m_descriptor_set_layouts.begin(),
            m_descriptor_set_layouts.end());
    }
    descriptor_set_allocate_info.descriptorSetCount = static_cast<uint32_t>(descriptor_set_layouts_for_frames.size());
    descriptor_set_allocate_info.pSetLayouts = descriptor_set_layouts_for_frames.data();

    // Vulkan can keep older command buffers in flight while the next frame rewrites descriptors.
    // Allocate one descriptor-set pack per frame slot so updates never clobber descriptors still
    // referenced by an in-flight frame.
    m_descriptor_sets_flat.resize(descriptor_set_layouts_for_frames.size());
    VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptor_set_allocate_info, m_descriptor_sets_flat.data()));

    m_descriptor_sets_per_frame.assign(m_descriptor_set_frame_slot_count, {});
    const size_t descriptor_set_count_per_frame = m_descriptor_set_layouts.size();
    for (unsigned frame_slot = 0; frame_slot < m_descriptor_set_frame_slot_count; ++frame_slot)
    {
        auto& frame_descriptor_sets = m_descriptor_sets_per_frame[frame_slot];
        frame_descriptor_sets.reserve(descriptor_set_count_per_frame);
        const size_t frame_begin_index = static_cast<size_t>(frame_slot) * descriptor_set_count_per_frame;
        for (size_t set_index = 0; set_index < descriptor_set_count_per_frame; ++set_index)
        {
            frame_descriptor_sets.push_back(m_descriptor_sets_flat[frame_begin_index + set_index]);
        }
    }
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

    if (!m_descriptor_sets_flat.empty())
    {
        auto& vk_descriptor_manager = dynamic_cast<VKDescriptorManager&>(memory_manager.GetDescriptorManager());
        const auto descriptor_pool = vk_descriptor_manager.GetDescriptorPool();
        if (descriptor_pool != VK_NULL_HANDLE)
        {
            VK_CHECK(vkFreeDescriptorSets(
                m_device,
                descriptor_pool,
                static_cast<uint32_t>(m_descriptor_sets_flat.size()),
                m_descriptor_sets_flat.data()));
        }
        m_descriptor_sets_flat.clear();
    }
    m_descriptor_sets_per_frame.clear();
    m_descriptor_set_frame_slot_count = 1;

    for (const auto& descriptor_set_layout : m_descriptor_set_layouts)
    {
        vkDestroyDescriptorSetLayout(m_device, descriptor_set_layout, nullptr);    
    }
    m_descriptor_set_layouts.clear();
    
    return true;
}

const std::vector<VkDescriptorSet>& VKRootSignature::GetDescriptorSetsForFrameSlot(unsigned frame_slot_index) const
{
    if (m_descriptor_sets_per_frame.empty())
    {
        return m_descriptor_sets_flat;
    }

    return m_descriptor_sets_per_frame[ResolveDescriptorSetFrameSlot(frame_slot_index)];
}

const std::vector<VkDescriptorSet>& VKRootSignature::GetDescriptorSetsForCommandList(const IRHICommandList& command_list) const
{
    return GetDescriptorSetsForFrameSlot(command_list.GetFrameSlotIndex());
}

const std::vector<VkDescriptorSetLayout>& VKRootSignature::GetDescriptorSetLayouts() const
{
    return m_descriptor_set_layouts;
}

unsigned VKRootSignature::GetDescriptorSetFrameSlotCount() const
{
    if (!m_descriptor_sets_per_frame.empty())
    {
        return static_cast<unsigned>(m_descriptor_sets_per_frame.size());
    }

    return m_descriptor_set_frame_slot_count;
}

unsigned VKRootSignature::ResolveDescriptorSetFrameSlot(unsigned frame_slot_index) const
{
    const unsigned frame_slot_count = GetDescriptorSetFrameSlotCount();
    return frame_slot_count > 0 ? (frame_slot_index % frame_slot_count) : 0;
}
