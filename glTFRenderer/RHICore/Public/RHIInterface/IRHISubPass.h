#pragma once
#include "RHIInterface/IRHIResource.h"
#include "RHICommon.h"

class RHICORE_API IRHISubPass : public IRHIResource
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHISubPass)
    
    virtual bool InitSubPass(const RHISubPassInfo& sub_pass_info) = 0;
};
