#pragma once
#include "RHIInterface/IRHIResource.h"

class RHICORE_API IRHIFactory : public IRHIResource
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIFactory)
    
    virtual bool InitFactory() = 0;
};
