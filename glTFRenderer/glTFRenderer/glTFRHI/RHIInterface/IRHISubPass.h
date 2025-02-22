#pragma once
#include "IRHIResource.h"
#include "RHICommon.h"

class IRHISubPass : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHISubPass)
    
    virtual bool InitSubPass(const RHISubPassInfo& sub_pass_info) = 0;
};
