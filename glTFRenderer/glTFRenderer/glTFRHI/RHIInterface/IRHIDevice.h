#pragma once
#include "IRHIFactory.h"
#include "../IRHIResource.h"

class IRHIDevice : public IRHIResource
{
public:
    virtual bool InitDevice(IRHIFactory& factory) = 0;
};
