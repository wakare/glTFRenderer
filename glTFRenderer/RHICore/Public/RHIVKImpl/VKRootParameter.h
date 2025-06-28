#pragma once

#include "RHIInterface/IRHIRootSignature.h"
#include "VolkUtils.h"

class VKRootParameter : public IRHIRootParameter
{
public:
    virtual bool InitAsConstant(unsigned constant_value, REGISTER_INDEX_TYPE register_index, unsigned space) override;
    virtual bool InitAsCBV(unsigned local_attribute_index, REGISTER_INDEX_TYPE register_index, unsigned space) override;
    virtual bool InitAsSRV(unsigned local_attribute_index, REGISTER_INDEX_TYPE register_index, unsigned space) override;
    virtual bool InitAsUAV(unsigned local_attribute_index, REGISTER_INDEX_TYPE register_index, unsigned space) override;
    virtual bool InitAsDescriptorTableRange(unsigned local_attribute_index,size_t range_count, const RHIDescriptorRangeDesc* range_desc) override;

    virtual bool IsBindless() const override;
    
    VkDescriptorSetLayoutBinding GetRawLayoutBinding() const;
    unsigned GetRegisterSpace() const;
    
protected:
    bool m_bindless{false};
    unsigned m_register_space{0};
    VkDescriptorSetLayoutBinding m_binding{};
};
