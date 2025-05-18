#pragma once
#include "IRHIFactory.h"
#include "IRHIResource.h"

class RHICORE_API IRHIDevice : public IRHIResource
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIDevice)
    
    virtual bool InitDevice(IRHIFactory& factory) = 0;
};
