#pragma once
#include "RHIInterface/IRHIRootSignature.h"
#include "VolkUtils.h"

class IRHICommandList;

class VKRootSignature : public IRHIRootSignature
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKRootSignature)
    
    virtual bool InitRootSignature(IRHIDevice& device, IRHIDescriptorManager& descriptor_manager) override;
    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
    const std::vector<VkDescriptorSet>& GetDescriptorSetsForFrameSlot(unsigned frame_slot_index) const;
    const std::vector<VkDescriptorSet>& GetDescriptorSetsForCommandList(const IRHICommandList& command_list) const;
    const std::vector<VkDescriptorSetLayout>& GetDescriptorSetLayouts() const;
    unsigned GetDescriptorSetFrameSlotCount() const;

protected:
    unsigned ResolveDescriptorSetFrameSlot(unsigned frame_slot_index) const;

    VkDevice m_device {VK_NULL_HANDLE};
    std::vector<VkDescriptorSetLayout> m_descriptor_set_layouts;
    std::vector<VkDescriptorSet> m_descriptor_sets_flat;
    std::vector<std::vector<VkDescriptorSet>> m_descriptor_sets_per_frame;
    unsigned m_descriptor_set_frame_slot_count {1};
};
