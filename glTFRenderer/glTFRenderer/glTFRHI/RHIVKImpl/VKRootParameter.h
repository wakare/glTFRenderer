#pragma once
#include "glTFRHI/RHIInterface/IRHIRootSignature.h"

class VKRootParameter : public IRHIRootParameter
{
public:
    virtual bool InitAsConstant(unsigned constantValue, REGISTER_INDEX_TYPE register_index, unsigned space) override;
    virtual bool InitAsCBV(REGISTER_INDEX_TYPE registerIndex, unsigned space) override;
    virtual bool InitAsSRV(REGISTER_INDEX_TYPE registerIndex, unsigned space) override;
    virtual bool InitAsUAV(REGISTER_INDEX_TYPE registerIndex, unsigned space) override;
    virtual bool InitAsDescriptorTableRange(size_t rangeCount, const RHIRootParameterDescriptorRangeDesc* rangeDesc) override;
};
