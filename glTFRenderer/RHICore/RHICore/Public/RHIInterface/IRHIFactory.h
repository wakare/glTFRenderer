#pragma once
#include "IRHIResource.h"

class IRHIFactory : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIFactory)
    
    virtual bool InitFactory() = 0;
};
