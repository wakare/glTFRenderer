#pragma once

#include "glTFRHI/RHIInterface/IRHIRootSignature.h"
#include <vulkan/vulkan_core.h>

class VKRootParameter : public IRHIRootParameter
{
public:
    virtual bool InitAsConstant(unsigned constant_value, REGISTER_INDEX_TYPE register_index, unsigned space) override;
    virtual bool InitAsCBV(unsigned attribute_index, REGISTER_INDEX_TYPE register_index, unsigned space) override;
    virtual bool InitAsSRV(unsigned attribute_index, REGISTER_INDEX_TYPE register_index, unsigned space) override;
    virtual bool InitAsUAV(unsigned attribute_index, REGISTER_INDEX_TYPE register_index, unsigned space) override;
    virtual bool InitAsDescriptorTableRange(unsigned attribute_index,size_t range_count, const RHIRootParameterDescriptorRangeDesc* range_desc) override;

    VkDescriptorSetLayoutBinding GetRawLayoutBinding() const;
    unsigned GetRegisterSpace() const;
    
protected:
    unsigned m_register_space{0};
    VkDescriptorSetLayoutBinding m_binding{};
};
