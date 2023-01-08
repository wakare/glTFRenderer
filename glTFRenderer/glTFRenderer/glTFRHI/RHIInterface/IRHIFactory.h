#pragma once
#include "../IRHIResource.h"

class IRHIFactory : public IRHIResource
{
public:
    virtual bool InitFactory() = 0;
    virtual ~IRHIFactory() = default;
};
