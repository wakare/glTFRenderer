#pragma once
#include "RHIInterface/IRHIRootSignature.h"
#include "VolkUtils.h"

class VKRootSignature : public IRHIRootSignature
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKRootSignature)
    
    virtual bool InitRootSignature(IRHIDevice& device, IRHIDescriptorManager& descriptor_manager) override;
    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
    const std::vector<VkDescriptorSet>& GetDescriptorSets() const;
    const std::vector<VkDescriptorSet>& GetDescriptorSets(unsigned frame_slot_index) const;
    const std::vector<VkDescriptorSetLayout>& GetDescriptorSetLayouts() const;

protected:
    static constexpr unsigned DESCRIPTOR_SET_FRAME_SLOT_COUNT = 4;

    VkDevice m_device {VK_NULL_HANDLE};
    std::vector<VkDescriptorSetLayout> m_descriptor_set_layouts;
    std::vector<VkDescriptorSet> m_descriptor_sets_flat;
    std::vector<std::vector<VkDescriptorSet>> m_descriptor_sets_per_frame;
};
