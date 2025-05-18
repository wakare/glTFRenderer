#pragma once
#include "VolkUtils.h"
#include "IRHIRootSignature.h"

class VKStaticSampler : public IRHIStaticSampler
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKStaticSampler)
    
    virtual bool InitStaticSampler(IRHIDevice& device, unsigned space, REGISTER_INDEX_TYPE register_index, RHIStaticSamplerAddressMode address_mode, RHIStaticSamplerFilterMode filter_mode) override;

    VkDescriptorSetLayoutBinding GetRawLayoutBinding() const;
    VkSampler GetRawSampler() const;
    unsigned GetRegisterSpace() const;

    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
protected:
    unsigned m_register_space{0};
    VkDescriptorSetLayoutBinding m_sampler_binding{};
    
    VkDevice m_device {VK_NULL_HANDLE};
    VkSampler m_sampler {VK_NULL_HANDLE};
};
