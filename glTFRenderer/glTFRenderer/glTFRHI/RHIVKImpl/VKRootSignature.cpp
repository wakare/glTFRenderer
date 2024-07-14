#include "VKRootSignature.h"

bool VKRootSignature::InitRootSignature(IRHIDevice& device)
{
    return true;
}

VkDescriptorSet VKRootSignature::GetDescriptorSet() const
{
    return m_descriptor_set;
}
