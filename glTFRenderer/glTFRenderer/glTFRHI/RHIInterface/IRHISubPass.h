#pragma once
#include "RHICommon.h"

class IRHISubPass
{
public:
    IRHISubPass() = default;
    virtual ~IRHISubPass() = default;
    
    virtual bool InitSubPass(const RHISubPassInfo& sub_pass_info) = 0;
};
