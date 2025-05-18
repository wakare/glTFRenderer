#pragma once
#include "IRHIFactory.h"
#include "IRHIResource.h"

class IRHIDevice : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIDevice)
    
    virtual bool InitDevice(IRHIFactory& factory) = 0;
};
