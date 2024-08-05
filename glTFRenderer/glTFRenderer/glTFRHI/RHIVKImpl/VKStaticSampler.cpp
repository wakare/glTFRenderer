#include "VKStaticSampler.h"

#include "VKDevice.h"

VKStaticSampler::~VKStaticSampler()
{
    vkDestroySampler(m_device, m_sampler, nullptr);
}

bool VKStaticSampler::InitStaticSampler(IRHIDevice& device, unsigned space, unsigned register_index, RHIStaticSamplerAddressMode address_mode,
                                        RHIStaticSamplerFilterMode filter_mode)
{
    VkSamplerCreateInfo sampler_desc{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    
    switch (address_mode) {
    case RHIStaticSamplerAddressMode::Warp:
        sampler_desc.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_desc.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_desc.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        break;
    case RHIStaticSamplerAddressMode::Mirror:
        sampler_desc.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        sampler_desc.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        sampler_desc.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        break;
    case RHIStaticSamplerAddressMode::Clamp:
        sampler_desc.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_desc.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_desc.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        break;
    case RHIStaticSamplerAddressMode::Border:
        sampler_desc.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler_desc.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler_desc.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        break;
    case RHIStaticSamplerAddressMode::MirrorOnce:
        sampler_desc.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
        sampler_desc.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
        sampler_desc.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
        break;
    case RHIStaticSamplerAddressMode::Unknown:
        GLTF_CHECK(false);
        break;
    }

    switch (filter_mode) {
    case RHIStaticSamplerFilterMode::Point:
        sampler_desc.magFilter = VK_FILTER_NEAREST;
        sampler_desc.minFilter = VK_FILTER_NEAREST;
        break;
    case RHIStaticSamplerFilterMode::Linear:
        sampler_desc.magFilter = VK_FILTER_LINEAR;
        sampler_desc.minFilter = VK_FILTER_LINEAR;
        break;
    case RHIStaticSamplerFilterMode::Anisotropic:
        GLTF_CHECK(false);
        break;
    case RHIStaticSamplerFilterMode::Unknown:
        GLTF_CHECK(false);
        break;
    }

    m_device = dynamic_cast<VKDevice&>(device).GetDevice();
    vkCreateSampler(m_device, &sampler_desc, nullptr, &m_sampler);
    
    m_sampler_binding.binding = register_index;
    m_sampler_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    m_sampler_binding.descriptorCount = 1;
    m_sampler_binding.stageFlags = VK_SHADER_STAGE_ALL;
    m_sampler_binding.pImmutableSamplers = &m_sampler;

    m_register_space = space;
    
    return true;
}

VkDescriptorSetLayoutBinding VKStaticSampler::GetRawLayoutBinding() const
{
    return m_sampler_binding;
}

VkSampler VKStaticSampler::GetRawSampler() const
{
    return m_sampler;
}

unsigned VKStaticSampler::GetRegisterSpace() const
{
    return m_register_space;
}
