#pragma once
#include "IRHIResource.h"
#include "RHICommon.h"

class IRHISubPass : public IRHIResource
{
public:
    IRHISubPass() = default;
    virtual ~IRHISubPass() = default;
    
    virtual bool InitSubPass(const RHISubPassInfo& sub_pass_info) = 0;
};
