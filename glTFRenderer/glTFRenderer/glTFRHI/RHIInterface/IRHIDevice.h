#pragma once
#include "IRHIFactory.h"
#include "IRHIResource.h"

class IRHIDevice : public IRHIResource
{
public:
    IRHIDevice() = default;
    DECLARE_NON_COPYABLE(IRHIDevice)
    virtual bool InitDevice(IRHIFactory& factory) = 0;
};
