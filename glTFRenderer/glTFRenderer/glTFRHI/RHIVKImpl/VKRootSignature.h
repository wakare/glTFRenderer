#pragma once
#include "glTFRHI/RHIInterface/IRHIRootSignature.h"
#include <vulkan/vulkan_core.h>

class VKRootSignature : public IRHIRootSignature
{
public:
    virtual bool InitRootSignature(IRHIDevice& device) override;

    VkDescriptorSet GetDescriptorSet() const;
    
protected:
    VkDescriptorSet m_descriptor_set {VK_NULL_HANDLE};
};
