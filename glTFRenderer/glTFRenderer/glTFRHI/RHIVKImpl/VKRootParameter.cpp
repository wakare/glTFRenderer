#include "VKRootParameter.h"

bool VKRootParameter::InitAsConstant(unsigned constant_value, unsigned register_index, unsigned space)
{
    m_register_space = space;
    GLTF_CHECK(false);
    return false;
}

bool VKRootParameter::InitAsCBV(unsigned register_index, unsigned space)
{
    m_register_space = space;
    
    m_binding.binding = register_index;
    m_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    m_binding.descriptorCount = 1;
    
    // TODO: optimization shader stage config
    m_binding.stageFlags = VK_SHADER_STAGE_ALL;
    return true;
}

bool VKRootParameter::InitAsSRV(unsigned register_index, unsigned space)
{
    m_register_space = space;
    
    m_binding.binding = register_index;
    m_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    m_binding.descriptorCount = 1;
    
    // TODO: optimization shader stage config
    m_binding.stageFlags = VK_SHADER_STAGE_ALL;
    
    return true;
}

bool VKRootParameter::InitAsUAV(unsigned register_index, unsigned space)
{
    m_register_space = space;
    
    m_binding.binding = register_index;
    m_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    m_binding.descriptorCount = 1;
    
    // TODO: optimization shader stage config
    m_binding.stageFlags = VK_SHADER_STAGE_ALL;
    
    return true;
}

bool VKRootParameter::InitAsDescriptorTableRange(size_t range_count,
    const RHIRootParameterDescriptorRangeDesc* range_desc)
{
    m_register_space = range_desc->space;
    
    m_binding.binding = range_desc->base_register_index;
    m_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    m_binding.descriptorCount = range_count;
    
    // TODO: optimization shader stage config
    m_binding.stageFlags = VK_SHADER_STAGE_ALL;
    
    return true;
}

VkDescriptorSetLayoutBinding VKRootParameter::GetRawLayoutBinding() const
{
    return m_binding;
}

unsigned VKRootParameter::GetRegisterSpace() const
{
    return m_register_space;
}
