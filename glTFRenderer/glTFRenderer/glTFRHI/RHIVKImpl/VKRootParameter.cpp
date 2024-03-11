#include "VKRootParameter.h"

bool VKRootParameter::InitAsConstant(unsigned constantValue, unsigned register_index, unsigned space)
{
    return true;
}

bool VKRootParameter::InitAsCBV(unsigned registerIndex, unsigned space)
{
    return true;
}

bool VKRootParameter::InitAsSRV(unsigned registerIndex, unsigned space)
{
    return true;
}

bool VKRootParameter::InitAsUAV(unsigned registerIndex, unsigned space)
{
    return true;
}

bool VKRootParameter::InitAsDescriptorTableRange(size_t rangeCount,
    const RHIRootParameterDescriptorRangeDesc* rangeDesc)
{
    return true;
}
