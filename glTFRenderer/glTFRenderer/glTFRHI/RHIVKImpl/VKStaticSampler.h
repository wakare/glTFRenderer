#pragma once
#include <vulkan/vulkan_core.h>

#include "glTFRHI/RHIInterface/IRHIRootSignature.h"

class VKStaticSampler : public IRHIStaticSampler
{
public:
    virtual bool InitStaticSampler(IRHIDevice& device, REGISTER_INDEX_TYPE register_index, RHIStaticSamplerAddressMode address_mode, RHIStaticSamplerFilterMode filter_mode) override;

    VkDescriptorSetLayoutBinding GetRawLayoutBinding() const;
    VkSampler GetRawSampler() const;
    
protected:
    VkDescriptorSetLayoutBinding m_sampler_binding{};
    VkSampler m_sampler{};
};
