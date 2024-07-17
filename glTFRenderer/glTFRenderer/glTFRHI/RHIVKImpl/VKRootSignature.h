#pragma once
#include "glTFRHI/RHIInterface/IRHIRootSignature.h"
#include <vulkan/vulkan_core.h>

class VKRootSignature : public IRHIRootSignature
{
public:
    virtual bool InitRootSignature(IRHIDevice& device, IRHIDescriptorManager& descriptor_manager) override;

    VkDescriptorSetLayout GetDescriptorSetLayout() const;
    VkDescriptorSet GetDescriptorSet() const;
    
protected:
    VkDescriptorSetLayout m_descriptor_set_layout {VK_NULL_HANDLE};
    VkDescriptorSet m_descriptor_set {VK_NULL_HANDLE};
};
