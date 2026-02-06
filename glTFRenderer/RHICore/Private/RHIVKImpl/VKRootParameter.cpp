#include "VKRootParameter.h"

bool VKRootParameter::InitAsConstant(unsigned constant_value, unsigned register_index, unsigned space)
{
    SetType(RHIRootParameterType::Constant);
    m_register_space = space;
    GLTF_CHECK(false);
    return false;
}

bool VKRootParameter::InitAsCBV(unsigned local_attribute_index, unsigned register_index, unsigned space)
{
    SetType(RHIRootParameterType::CBV);
    m_register_space = space;
    
    m_binding.binding = local_attribute_index;
    m_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    m_binding.descriptorCount = 1;
    
    // TODO: optimization shader stage config
    m_binding.stageFlags = VK_SHADER_STAGE_ALL;
    return true;
}

bool VKRootParameter::InitAsSRV(unsigned local_attribute_index, unsigned register_index, unsigned space)
{
    SetType(RHIRootParameterType::SRV);
    m_register_space = space;
    
    m_binding.binding = local_attribute_index;
    m_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    m_binding.descriptorCount = 1;
    
    // TODO: optimization shader stage config
    m_binding.stageFlags = VK_SHADER_STAGE_ALL;
    
    return true;
}

bool VKRootParameter::InitAsUAV(unsigned local_attribute_index, unsigned register_index, unsigned space)
{
    SetType(RHIRootParameterType::UAV);
    m_register_space = space;
    
    m_binding.binding = local_attribute_index;
    m_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    m_binding.descriptorCount = 1;
    
    // TODO: optimization shader stage config
    m_binding.stageFlags = VK_SHADER_STAGE_ALL;
    
    return true;
}

bool VKRootParameter::InitAsAccelerationStructure(unsigned local_attribute_index, unsigned register_index, unsigned space)
{
    SetType(RHIRootParameterType::AccelerationStructure);
    m_register_space = space;

    m_binding.binding = local_attribute_index;
    m_binding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    m_binding.descriptorCount = 1;

    // TODO: optimization shader stage config
    m_binding.stageFlags = VK_SHADER_STAGE_ALL;

    return true;
}

bool VKRootParameter::InitAsDescriptorTableRange(unsigned local_attribute_index, size_t range_count,
    const RHIDescriptorRangeDesc* range_desc)
{
    SetType(RHIRootParameterType::DescriptorTable);
    
    m_register_space = range_desc->space;
    m_bindless = (range_desc->descriptor_count == UINT_MAX);
    
    m_binding.binding = local_attribute_index;
    m_binding.descriptorType = range_desc->type == RHIDescriptorRangeType::SRV ?
        range_desc->is_buffer ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE :
        range_desc->is_buffer ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ;
    m_binding.descriptorCount = m_bindless ? 1024 : range_desc->descriptor_count;
    
    m_binding.stageFlags = VK_SHADER_STAGE_ALL;
    
    return true;
}

bool VKRootParameter::IsBindless() const
{
    return m_bindless;
}

VkDescriptorSetLayoutBinding VKRootParameter::GetRawLayoutBinding() const
{
    return m_binding;
}

unsigned VKRootParameter::GetRegisterSpace() const
{
    return m_register_space;
}
