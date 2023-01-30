#pragma once

#include "RHICommon.h"

class IRHIResource
{
    friend class RHIResourceFactory;
public:
    IRHIResource() = default;

    // Clean resource in virtual dtor!!
    virtual ~IRHIResource() = 0;
    
private:
    // Disable copy and move ctor and assignment operation
    IRHIResource(const IRHIResource&) = default;
    IRHIResource& operator=(const IRHIResource&) = default;

    IRHIResource(IRHIResource&&) = default;
    IRHIResource& operator=(IRHIResource&&) = default;
};
