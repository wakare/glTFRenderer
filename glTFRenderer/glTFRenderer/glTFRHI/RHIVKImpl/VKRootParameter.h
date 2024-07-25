#pragma once

#include "glTFRHI/RHIInterface/IRHIRootSignature.h"
#include <vulkan/vulkan_core.h>

class VKRootParameter : public IRHIRootParameter
{
public:
    virtual bool InitAsConstant(unsigned constant_value, REGISTER_INDEX_TYPE register_index, unsigned space) override;
    virtual bool InitAsCBV(REGISTER_INDEX_TYPE register_index, unsigned space) override;
    virtual bool InitAsSRV(REGISTER_INDEX_TYPE register_index, unsigned space) override;
    virtual bool InitAsUAV(REGISTER_INDEX_TYPE register_index, unsigned space) override;
    virtual bool InitAsDescriptorTableRange(size_t range_count, const RHIRootParameterDescriptorRangeDesc* range_desc) override;

    VkDescriptorSetLayoutBinding GetRawLayoutBinding() const;
    
protected:
    VkDescriptorSetLayoutBinding m_binding{};
};
