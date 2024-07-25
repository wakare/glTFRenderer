#pragma once
#include "glTFRHI/RHIInterface/IRHIRootSignature.h"
#include <vulkan/vulkan_core.h>

class VKRootSignature : public IRHIRootSignature
{
public:
    virtual bool InitRootSignature(IRHIDevice& device, IRHIDescriptorManager& descriptor_manager) override;

    VkDescriptorSetLayout GetDescriptorSetLayoutResource() const;
    VkDescriptorSetLayout GetDescriptorSetLayoutSampler() const;

    VkDescriptorSet GetDescriptorSetResource() const;
    VkDescriptorSet GetDescriptorSetSampler() const;
    
protected:
    VkDescriptorSetLayout m_descriptor_set_layout_resource {VK_NULL_HANDLE};
    VkDescriptorSetLayout m_descriptor_set_layout_sampler {VK_NULL_HANDLE};
    VkDescriptorSet m_descriptor_set_resource {VK_NULL_HANDLE};
    VkDescriptorSet m_descriptor_set_sampler {VK_NULL_HANDLE};
};
