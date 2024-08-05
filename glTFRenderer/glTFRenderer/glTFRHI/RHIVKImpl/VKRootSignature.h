#pragma once
#include "glTFRHI/RHIInterface/IRHIRootSignature.h"
#include "VolkUtils.h"

class VKRootSignature : public IRHIRootSignature
{
public:
    virtual ~VKRootSignature() override;
    virtual bool InitRootSignature(IRHIDevice& device, IRHIDescriptorManager& descriptor_manager) override;

    const std::vector<VkDescriptorSet>& GetDescriptorSets() const;
    const std::vector<VkDescriptorSetLayout>& GetDescriptorSetLayouts() const;
    
protected:
    VkDevice m_device {VK_NULL_HANDLE};
    std::vector<VkDescriptorSetLayout> m_descriptor_set_layouts;
    std::vector<VkDescriptorSet> m_descriptor_sets;
};
