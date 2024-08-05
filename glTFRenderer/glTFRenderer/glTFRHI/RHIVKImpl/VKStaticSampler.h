#pragma once
#include "VolkUtils.h"
#include "glTFRHI/RHIInterface/IRHIRootSignature.h"

class VKStaticSampler : public IRHIStaticSampler
{
public:
    virtual ~VKStaticSampler() override;
    virtual bool InitStaticSampler(IRHIDevice& device, unsigned space, REGISTER_INDEX_TYPE register_index, RHIStaticSamplerAddressMode address_mode, RHIStaticSamplerFilterMode filter_mode) override;

    VkDescriptorSetLayoutBinding GetRawLayoutBinding() const;
    VkSampler GetRawSampler() const;
    unsigned GetRegisterSpace() const;
    
protected:
    unsigned m_register_space{0};
    VkDescriptorSetLayoutBinding m_sampler_binding{};
    
    VkDevice m_device {VK_NULL_HANDLE};
    VkSampler m_sampler {VK_NULL_HANDLE};
};
